#include "meshcore_plugin.h"

struct MESHCORE_USER_PUBKEY {
	string pubkey;
	int64 lastHit = 0;
};
map<string, MESHCORE_USER_PUBKEY> pubkeys;

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
	MESHCORE_USER_PUBKEY pk;
	pk.pubkey = pubkey;
	pk.lastHit = time(NULL);
	char* p = strdup(nick.c_str());
	strlwr(p);
	AutoMutex(hMutex);
	pubkeys[p] = std::move(pk);
	free(p);
}

void maintainCache() {
	AutoMutex(hMutex);
	if (pubkeys.size() >= 100) {
		while (pubkeys.size() > 90) { // clear 10 oldest
			int64 oldest = INT64_MAX;
			map<string, MESHCORE_USER_PUBKEY>::iterator toDel = pubkeys.end();
			for (auto x = pubkeys.begin(); x != pubkeys.end(); x++) {
				if (x->second.lastHit < oldest) {
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

	char* p = strdup(nick.c_str());
	strlwr(p);
	AutoMutex(hMutex);
	auto x = pubkeys.find(p);
	if (x != pubkeys.end()) {
		pubkeys.erase(x);
	}
	free(p);

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

bool GetUserPubKey(const string& nick, string& pubkey) {
	{
		AutoMutex(hMutex);
		maintainCache();

		char* p = strdup(nick.c_str());
		strlwr(p);
		auto x = pubkeys.find(p);
		free(p);
		if (x != pubkeys.end()) {
			x->second.lastHit = time(NULL);
			pubkey = x->second.pubkey;
			return !pubkey.empty();
		}
	}

	char* q = api->db->MPrintf("SELECT PubKey FROM meshcore_users WHERE Nick=%Q", nick.c_str());
	sql_rows res = GetTable(q);
	api->db->Free(q);
	if (res.size() > 0) {
		pubkey = res.begin()->second["PubKey"];
		cachePubKey(nick, pubkey);
		return !pubkey.empty();
	} else {
		cachePubKey(nick, "");
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
		if (GetUserPubKey(parms, pk)) {
			snprintf(buf, sizeof(buf), _("Public key for %s: %s"), parms, pk.c_str());
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
			snprintf(buf, sizeof(buf), _("Public key saved for %s: %s"), parms, pk.c_str());
			ut->std_reply(ut, buf);
		} else {
			snprintf(buf, sizeof(buf), _("Error saving public key for %s!"), parms);
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	return 0;
}
