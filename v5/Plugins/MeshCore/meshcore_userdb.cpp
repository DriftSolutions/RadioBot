#include "meshcore_plugin.h"

string GetPubKeyPrefix(const string& pubkey) {
	return pubkey.substr(0, 12);
}

struct MESHCORE_USER_PUBKEY {
	set<string> pubkeys;
	int64 lastHit = 0;
	bool is_manual = false;

	string GetPubKey() {
		if (pubkeys.size() == 1) {
			return *pubkeys.begin();
		}
		return "";
	}

	string GetPubKeyPrefix() {
		if (pubkeys.size() == 1) {
			return ::GetPubKeyPrefix(*pubkeys.begin());
		}
		return "";
	}

	void RemovePubKey(const string& pubkey) {
		auto x = pubkeys.find(pubkey);
		if (x != pubkeys.end()) {
			pubkeys.erase(x);
		}
	}
};

map<string, MESHCORE_USER_PUBKEY, ci_less> pubkeys;

string get_sanitized_nick(const string& str) {
	char* nick = strdup(str.c_str());

	str_replaceA(nick, strlen(nick) + 1, " ", "_");
	str_replaceA(nick, strlen(nick) + 1, "!", "_");
	str_replaceA(nick, strlen(nick) + 1, "@", "_");

	char* p = nick;
	while (*p) {
		if (!isprint(*p)) {
			memmove(p, p + 1, strlen(p));
		} else {
			p++;
		}
	}

	strtrim(nick);
	string ret = nick;
	free(nick);

	return ret;
}

void AddPubKey(const string& nick, const string& pubkey, bool is_manual) {
	if (nick.empty() || pubkey.empty()) {
		return;
	}

	string p = get_sanitized_nick(nick);
	if (p.empty()) {
		return;
	}

	AutoMutex(hMutex);
	auto x = pubkeys.find(p);
	if (x != pubkeys.end()) {
		x->second.lastHit = time(NULL);
		if (is_manual) {
#ifdef DEBUG
			api->ib_printf(_("MeshCore -> Manually set pubkey for %s (%s): %s\n"), nick.c_str(), p.c_str(), pubkey.c_str());
#endif
			x->second.pubkeys = { pubkey };
			x->second.is_manual = is_manual;
		} else {
			x->second.pubkeys.insert(pubkey);
#ifdef DEBUG
			api->ib_printf(_("MeshCore -> Updated pubkey for %s (%s): %s\n"), nick.c_str(), p.c_str(), pubkey.c_str());
#endif
			if (x->second.pubkeys.size() > 1) {
				api->ib_printf(_("MeshCore -> WARNING: Nick %s (%s) has more than one pubkey so they may be spoofing!\n"), nick.c_str(), p.c_str());
			}
		}
	} else {
		MESHCORE_USER_PUBKEY pk;
		pk.lastHit = time(NULL);
		pk.pubkeys = { pubkey };
		pk.is_manual = is_manual;
		pubkeys[p] = pk;
#ifdef DEBUG
		api->ib_printf(_("MeshCore -> Added new pubkey for %s (%s): %s\n"), nick.c_str(), p.c_str(), pubkey.c_str());
#endif
	}

	for (x = pubkeys.begin(); x != pubkeys.end(); x++) {
		if (stricmp(x->first.c_str(), p.c_str())) {
			// Remove from any other records
			x->second.RemovePubKey(pubkey);
		}
	}

	if (pubkeys.size() > 1024) {
		int tries = 0;
		while (pubkeys.size() > 1000 && tries++ < 25) { // clear oldest
			int64 oldest = INT64_MAX;
			map<string, MESHCORE_USER_PUBKEY>::iterator toDel = pubkeys.end();
			for (auto x = pubkeys.begin(); x != pubkeys.end(); x++) {
				if (x->second.lastHit < oldest && !x->second.is_manual) {
					oldest = x->second.lastHit;
					toDel = x;
				}
			}
			if (toDel != pubkeys.end()) {
				pubkeys.erase(toDel);
			} else {
				// should never happen
				break;
			}
		}
	}
}

sql_rows GetTable(const char* query, char** errmsg) {
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

bool SaveUserPubKey(const string& nick, string& pubkey) {
	char* q = api->db->MPrintf("REPLACE INTO meshcore_users (`Nick`,`PubKey`) VALUES (%Q, %Q)", nick.c_str(), pubkey.c_str());
	bool ret = (api->db->Query(q, NULL, NULL, NULL) == SQLITE_OK);
	api->db->Free(q);

	if (ret) {
		AddPubKey(nick, pubkey, true);
	}

	return ret;
}

bool DelUserPubKey(const string& nick) {
	char* q = api->db->MPrintf("DELETE FROM meshcore_users WHERE Nick=%Q", nick.c_str());
	bool ret = (api->db->Query(q, NULL, NULL, NULL) == SQLITE_OK);
	api->db->Free(q);

	string p = get_sanitized_nick(nick);
	AutoMutex(hMutex);
	auto x = pubkeys.find(p);
	if (x != pubkeys.end()) {
		pubkeys.erase(x);
	}

	return ret;
}

bool GetUserPubKey(const string& nick, string& pubkey, bool *was_from_db) {
	string p = get_sanitized_nick(nick);

	AutoMutex(hMutex);

	auto x = pubkeys.find(p);
	if (x != pubkeys.end()) {
		x->second.lastHit = time(NULL);
		if (x->second.pubkeys.size() == 1) {
			// only return a pubkey if only one person with that nick has been seen
			pubkey = *x->second.pubkeys.begin();
			if (was_from_db != NULL) {
				*was_from_db = x->second.is_manual;
			}
#ifdef DEBUG
			api->ib_printf(_("MeshCore -> Found pubkey for %s\n"), p.c_str(), pubkey.c_str());
#endif
			return true;
		}
	}

	return false;
}

bool GetNickFromPubKeyPrefix(const string& pubkey, string& nick) {
	if (pubkey.empty()) { return false; }

	for (auto& x : pubkeys) {
		if (!stricmp(pubkey.c_str(), x.second.GetPubKeyPrefix().c_str())) {
			nick = x.first;
			return true;
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
