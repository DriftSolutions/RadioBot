#include "meshcore_plugin.h"

struct MESHCORE_USER_PUBKEY {
	set<string> pubkeys;
	int64 lastHit = 0;
};

map<string, MESHCORE_USER_PUBKEY> db_pubkeys;
map<string, MESHCORE_USER_PUBKEY> live_pubkeys;

string get_sanitized_nick(const string& str) {
	char* p = strdup(str.c_str());
	sanitize_nick(p);
	strlwr(p);
	string ret = p;
	free(p);
	return ret;
}

void AddLivePubKey(const string& nick, const string& pubkey) {
	if (nick.empty() || pubkey.empty()) {
		return;
	}

	string p = get_sanitized_nick(nick);
	if (p.empty()) {
		return;
	}

	AutoMutex(hMutex);
	auto x = live_pubkeys.find(p);
	if (x != live_pubkeys.end()) {
		x->second.lastHit = time(NULL);
		x->second.pubkeys.insert(pubkey);
#ifdef DEBUG
		api->ib_printf(_("MeshCore -> Updated live pubkey for %s (%s): %s\n"), nick.c_str(), p.c_str(), pubkey.c_str());
#endif
		if (x->second.pubkeys.size() > 1) {
			api->ib_printf(_("MeshCore -> WARNING: Nick % has more than one pubkey so they may be spoofing!\n"), nick.c_str());
		}
	} else {
		MESHCORE_USER_PUBKEY pk;
		pk.lastHit = time(NULL);
		pk.pubkeys = { pubkey };
		live_pubkeys[p] = std::move(pk);
#ifdef DEBUG
		api->ib_printf(_("MeshCore -> Added new live pubkey for %s (%s): %s\n"), nick.c_str(), p.c_str(), pubkey.c_str());
#endif
	}

#ifdef DEBUG
	api->ib_printf(_("MeshCore -> Live cache size: %zu records\n"), live_pubkeys.size());
#endif

	if (live_pubkeys.size() > 1024) {
		while (live_pubkeys.size() > 1000) { // clear oldest
			int64 oldest = INT64_MAX;
			map<string, MESHCORE_USER_PUBKEY>::iterator toDel = live_pubkeys.end();
			for (auto x = live_pubkeys.begin(); x != live_pubkeys.end(); x++) {
				if (x->second.lastHit < oldest) {
					oldest = x->second.lastHit;
					toDel = x;
				}
			}
			if (toDel != live_pubkeys.end()) {
				live_pubkeys.erase(toDel);
			} else {
				// should never happen
				break;
			}
		}
	}
}

sql_rows GetTable(const char* query, char** errmsg = NULL) {
	//int GetTable(char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char* errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	int nrow = 0, ncol = 0;
	char** resultp;
	while ((ret = api->db->GetTable(query, &resultp, &nrow, &ncol, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		api->ib_printf(_("GetTable: %s -> %d (%s)\n"), query, ret, *errmsg2);
	} else {
		if (ncol && nrow) {
			for (int i = 0; i < nrow; i++) {
				sql_row row;
				for (int s = 0; s < ncol; s++) {
					char* p = resultp[(i * ncol) + ncol + s];
					if (!p) { p = ""; }
					row[resultp[s]] = p;
				}
				rows[i] = row;
			}
		}
		api->db->FreeTable(resultp);
		//azResult0 = "Name";
		//azResult1 = "Age";
	}
	if (errmsg == NULL && *errmsg2) {
		api->db->Free(*errmsg2);
	}
	return rows;
}

void cachePubKey(const string& nick, const string& pubkey) {
	if (nick.empty() || pubkey.empty()) {
		return;
	}

	MESHCORE_USER_PUBKEY pk;
	pk.pubkeys = { pubkey };
	pk.lastHit = time(NULL);
	string p = get_sanitized_nick(nick);
	AutoMutex(hMutex);
	db_pubkeys[p] = std::move(pk);
}

void maintainCache() {
	AutoMutex(hMutex);
	if (db_pubkeys.size() >= 100) {
		while (db_pubkeys.size() > 90) { // clear 10 oldest
			int64 oldest = INT64_MAX;
			map<string, MESHCORE_USER_PUBKEY>::iterator toDel = db_pubkeys.end();
			for (auto x = db_pubkeys.begin(); x != db_pubkeys.end(); x++) {
				if (x->second.lastHit < oldest) {
					oldest = x->second.lastHit;
					toDel = x;
				}
			}
			if (toDel != db_pubkeys.end()) {
				db_pubkeys.erase(toDel);			
			} else {
				// should never happen
				break;
			}
		}
	}
}

bool SaveUserPubKey(const string& nick, string& pubkey) {
	char* q = api->db->MPrintf("REPLACE INTO meshcore_users (`Nick`,`PubKey`) VALUES (%Q, %Q)", nick.c_str(), pubkey.c_str());
	bool ret = (api->db->Query(q, NULL, NULL, NULL) == SQLITE_OK);
	api->db->Free(q);

	if (ret) {
		cachePubKey(nick, pubkey);		
	}

	return ret;
}

bool DelUserPubKey(const string& nick) {
	char* q = api->db->MPrintf("DELETE FROM meshcore_users WHERE Nick=%Q", nick.c_str());
	bool ret = (api->db->Query(q, NULL, NULL, NULL) == SQLITE_OK);
	api->db->Free(q);

	string p = get_sanitized_nick(nick);
	AutoMutex(hMutex);
	auto x = db_pubkeys.find(p);
	if (x != db_pubkeys.end()) {
		db_pubkeys.erase(x);
	}

	return ret;
}
/*
bool DelQuote(uint32 id) {
	std::stringstream sstr;
	sstr << "DELETE FROM quotes WHERE ID=" << id;
	if (api->db->Query(sstr.str().c_str(), NULL, NULL, NULL) == SQLITE_OK) {
		return true;
	}
	return false;
}
*/

bool GetUserPubKey(const string& nick, string& pubkey, bool *was_from_db) {
	string p = get_sanitized_nick(nick);

	// First, check if a pubkey has been cached from the DB
	{
		AutoMutex(hMutex);
		maintainCache();

		auto x = db_pubkeys.find(p);
		if (x != db_pubkeys.end()) {
			x->second.lastHit = time(NULL);
			pubkey = *x->second.pubkeys.begin();
			if (was_from_db != NULL) {
				*was_from_db = true;
			}
#ifdef DEBUG
			api->ib_printf(_("MeshCore -> Found pubkey for %s in DB cache\n"), p.c_str(), pubkey.c_str());
#endif
			return true;
		}
	}

	// Second, check if a pubkey has been saved in the DB
	char* q = api->db->MPrintf("SELECT PubKey FROM meshcore_users WHERE Nick=%Q", p.c_str());
	sql_rows res = GetTable(q);
	api->db->Free(q);
	if (res.size() > 0) {
		pubkey = res.begin()->second["PubKey"];
		cachePubKey(nick, pubkey);
		if (was_from_db != NULL) {
			*was_from_db = true;
		}
#ifdef DEBUG
		api->ib_printf(_("MeshCore -> Found pubkey for %s in DB\n"), p.c_str(), pubkey.c_str());
#endif
		return true;
	}

	// Third, see if it's known from the live data
	{
		AutoMutex(hMutex);

		auto x = live_pubkeys.find(p);
		if (x != live_pubkeys.end()) {
			x->second.lastHit = time(NULL);
			if (x->second.pubkeys.size() == 1) {
				// only return a live pubkey if only one person with that nick has been seen
				pubkey = *x->second.pubkeys.begin();
				if (was_from_db != NULL) {
					*was_from_db = false;
				}
#ifdef DEBUG
				api->ib_printf(_("MeshCore -> Found pubkey for %s in live data\n"), p.c_str(), pubkey.c_str());
#endif
				return true;
			}
		}
	}

	return false;
}

int MeshCore_UserDB_Commands(const char* command, const char* parms, USER_PRESENCE* ut, uint32 type) {
	char buf[1024];

	if (!stricmp(command, "meshcore-viewuserpubkey")) {
		if (!parms || !strlen(parms)) {
			snprintf(buf, sizeof(buf), _("Usage: %cmeshcore-viewuserpubkey nickname"), api->commands->PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			return 1;
		}

		string pk;
		bool was_from_db;
		if (GetUserPubKey(parms, pk, &was_from_db)) {
			snprintf(buf, sizeof(buf), _("Public key for %s: %s [source: %s]"), parms, pk.c_str(), was_from_db ? "saved" : "live data");
			ut->std_reply(ut, buf);
		} else {
			snprintf(buf, sizeof(buf), _("No public key stored for %s!"), parms);
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	if (!stricmp(command, "meshcore-deluserpubkey")) {
		if (!parms || !strlen(parms)) {
			snprintf(buf, sizeof(buf), _("Usage: %cmeshcore-deluserpubkey nickname"), api->commands->PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			return 1;
		}

		if (DelUserPubKey(parms)) {
			snprintf(buf, sizeof(buf), _("Public key deleted for %s"), parms);
			ut->std_reply(ut, buf);
		} else {
			snprintf(buf, sizeof(buf), _("Error deleting public key for %s!"), parms);
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	if (!stricmp(command, "meshcore-saveuserpubkey")) {
		if (!parms || !strlen(parms)) {
			snprintf(buf, sizeof(buf), _("Usage: %cmeshcore-saveuserpubkey nickname pubkey"), api->commands->PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			return 1;
		}

		StrTokenizer st((char *)parms, ' ');
		if (st.NumTok() != 2) {
			snprintf(buf, sizeof(buf), _("Usage: %cmeshcore-saveuserpubkey nickname pubkey"), api->commands->PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			return 1;
		}

		string nick = st.stdGetSingleTok(1);
		string pk = st.stdGetSingleTok(2);
		if (strspn(pk.c_str(), "0123456789abcdef") != pk.size() || pk.size() % 2 != 0) {
			ut->std_reply(ut, _("That doesn't appear to be a valid public key!"));
			return 1;
		}

		if (SaveUserPubKey(nick, pk)) {
			snprintf(buf, sizeof(buf), _("Public key saved for %s: %s"), nick.c_str(), pk.c_str());
			ut->std_reply(ut, buf);
		} else {
			snprintf(buf, sizeof(buf), _("Error saving public key for %s!"), nick.c_str());
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	return 0;
}
