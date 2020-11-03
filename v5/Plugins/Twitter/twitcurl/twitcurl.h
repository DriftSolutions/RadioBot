#ifndef _TWITCURL_H_
#define _TWITCURL_H_

#include <string>
#include "../twitter.h"
#include "oauthlib.h"
//#include "curl/curl.h"

/* Default values used in twitcurl */
namespace twitCurlDefaults
{
    /* Constants */
    const int TWITCURL_DEFAULT_BUFFSIZE = 1024;
    const std::string TWITCURL_COLON = ":";
    const char TWITCURL_EOS = '\0';

    /* Miscellaneous data used to build twitter URLs*/
    const std::string TWITCURL_STATUSSTRING = "status=";
    const std::string TWITCURL_TEXTSTRING = "text=";
    const std::string TWITCURL_QUERYSTRING = "query=";  
    const std::string TWITCURL_SEARCHQUERYSTRING = "?q=";      
    const std::string TWITCURL_SCREENNAME = "?screen_name=";
    const std::string TWITCURL_USERID = "?user_id=";
    const std::string TWITCURL_EXTENSIONFORMAT = ".json";
    const std::string TWITCURL_TARGETSCREENNAME = "?target_screen_name=";
    const std::string TWITCURL_TARGETUSERID = "?target_id=";
};

/* Default twitter URLs */
namespace twitterDefaults
{

    /* Search URLs */
    const std::string TWITCURL_SEARCH_URL = "http://search.twitter.com/search.atom";

    /* Status URLs */
    const std::string TWITCURL_STATUSUPDATE_URL = "https://api.twitter.com/1.1/statuses/update.json";
    const std::string TWITCURL_STATUSSHOW_URL = "https://api.twitter.com/1.1/statuses/show/";
    const std::string TWITCURL_STATUDESTROY_URL = "https://api.twitter.com/1.1/statuses/destroy/";

    /* Timeline URLs */
    const std::string TWITCURL_PUBLIC_TIMELINE_URL = "https://api.twitter.com/1.1/statuses/public_timeline.json";
    const std::string TWITCURL_FEATURED_USERS_URL = "https://api.twitter.com/1.1/statuses/featured.json";
    const std::string TWITCURL_FRIENDS_TIMELINE_URL = "https://api.twitter.com/1.1/statuses/friends_timeline.json";
    const std::string TWITCURL_HOME_TIMELINE_URL = "https://api.twitter.com/1.1/statuses/home_timeline.json";
    const std::string TWITCURL_MENTIONS_URL = "https://api.twitter.com/1.1/statuses/mentions_timeline.json";
    const std::string TWITCURL_USERTIMELINE_URL = "https://api.twitter.com/1.1/statuses/user_timeline.json";

    /* Users URLs */
    const std::string TWITCURL_SHOWUSERS_URL = "https://api.twitter.com/1.1/users/show.json";
    const std::string TWITCURL_SHOWFRIENDS_URL = "https://api.twitter.com/1.1/statuses/friends.json";
    const std::string TWITCURL_SHOWFOLLOWERS_URL = "https://api.twitter.com/1.1/statuses/followers.json";

    /* Direct messages URLs */
    const std::string TWITCURL_DIRECTMESSAGES_URL = "https://api.twitter.com/1.1/direct_messages/events/list.json";
    const std::string TWITCURL_DIRECTMESSAGENEW_URL = "https://api.twitter.com/1.1/direct_messages/events/new.json";
    const std::string TWITCURL_DIRECTMESSAGESSENT_URL = "https://api.twitter.com/1.1/direct_messages/sent.json";
    const std::string TWITCURL_DIRECTMESSAGEDESTROY_URL = "https://api.twitter.com/1.1/direct_messages/destroy/";

    /* Friendships URLs */
    const std::string TWITCURL_FRIENDSHIPSCREATE_URL = "https://api.twitter.com/1.1/friendships/create.json";
    const std::string TWITCURL_FRIENDSHIPSDESTROY_URL = "https://api.twitter.com/1.1/friendships/destroy.json";
    const std::string TWITCURL_FRIENDSHIPSSHOW_URL = "https://api.twitter.com/1.1/friendships/show.json";

    /* Social graphs URLs */
    const std::string TWITCURL_FRIENDSIDS_URL = "https://api.twitter.com/1.1/friends/ids.json";
    const std::string TWITCURL_FOLLOWERSIDS_URL = "https://api.twitter.com/1.1/followers/ids.json";

    /* Account URLs */
    const std::string TWITCURL_ACCOUNTRATELIMIT_URL = "https://api.twitter.com/1.1/account/rate_limit_status.json";

    /* Favorites URLs */
    const std::string TWITCURL_FAVORITESGET_URL = "https://api.twitter.com/1.1/favorites.json";
    const std::string TWITCURL_FAVORITECREATE_URL = "https://api.twitter.com/1.1/favorites/create/";
    const std::string TWITCURL_FAVORITEDESTROY_URL = "https://api.twitter.com/1.1/favorites/destroy/";

    /* Block URLs */
    const std::string TWITCURL_BLOCKSCREATE_URL = "https://api.twitter.com/1.1/blocks/create/";
    const std::string TWITCURL_BLOCKSDESTROY_URL = "https://api.twitter.com/1.1/blocks/destroy/";
    
    /* Saved Search URLs */
    const std::string TWITCURL_SAVEDSEARCHGET_URL = "https://api.twitter.com/1.1/saved_searches.json";
    const std::string TWITCURL_SAVEDSEARCHSHOW_URL = "https://api.twitter.com/1.1/saved_searches/show/";
    const std::string TWITCURL_SAVEDSEARCHCREATE_URL = "https://api.twitter.com/1.1/saved_searches/create.json";
    const std::string TWITCURL_SAVEDSEARCHDESTROY_URL = "https://api.twitter.com/1.1/saved_searches/destroy/";
    
};

/* twitCurl class */
class twitCurl
{
public:
    twitCurl();
    ~twitCurl();

    /* Twitter OAuth authorization methods */
    oAuth& getOAuth();
    bool oAuthRequestToken( std::string& authorizeUrl /* out */ );
    bool oAuthAccessToken();

    /* Twitter login APIs, set once and forget */
    std::string& getTwitterUsername();
    std::string& getTwitterPassword();
    void setTwitterUsername( std::string& userName /* in */ );
    void setTwitterPassword( std::string& passWord /* in */ );

	bool getUserInfoByID(int64 user_id);
	bool getUserInfoByScreenName(std::string screen_name);

    /* Twitter search APIs */
    bool search( std::string& query /* in */ );

    /* Twitter status APIs */
    bool statusUpdate( std::string& newStatus /* in */ );
    bool statusShowById( std::string& statusId /* in */ );
    bool statusDestroyById( std::string& statusId /* in */ );

    /* Twitter timeline APIs */
    bool timelinePublicGet();
    bool timelineFriendsGet();
	bool timelineHomeGet(std::string parms = "");
    bool timelineUserGet( std::string userInfo = "" /* in */, bool isUserId = false /* in */ );
    bool featuredUsersGet();
    bool mentionsGet(std::string parms = "");

    /* Twitter user APIs */
    bool userGet( std::string& userInfo /* in */, bool isUserId = false /* in */ );
    bool friendsGet( std::string userInfo = "" /* in */, bool isUserId = false /* in */ );
    bool followersGet( std::string userInfo = "" /* in */, bool isUserId = false /* in */ );

    /* Twitter direct message APIs */
    bool directMessageGet(std::string parms = "");
    bool directMessageSend(uint64_t user_id, std::string& dMsg);
    bool directMessageGetSent();
    bool directMessageDestroyById( std::string& dMsgId /* in */ );

    /* Twitter friendships APIs */
    bool friendshipCreate( std::string& userInfo /* in */, bool isUserId = false /* in */ );
    bool friendshipDestroy( std::string& userInfo /* in */, bool isUserId = false /* in */ );
    bool friendshipShow( std::string& userInfo /* in */, bool isUserId = false /* in */ );

    /* Twitter social graphs APIs */
    bool friendsIdsGet( std::string& userInfo /* in */, bool isUserId = false /* in */ );
    bool followersIdsGet( std::string& userInfo /* in */, bool isUserId = false /* in */ );

    /* Twitter account APIs */
    bool accountRateLimitGet();

    /* Twitter favorites APIs */
    bool favoriteGet();
    bool favoriteCreate( std::string& statusId /* in */ );
    bool favoriteDestroy( std::string& statusId /* in */ );

    /* Twitter block APIs */
    bool blockCreate( std::string& userInfo /* in */ );
    bool blockDestroy( std::string& userInfo /* in */ );

    /* Twitter search APIs */
    bool savedSearchGet();
    bool savedSearchCreate( std::string& query /* in */ );
    bool savedSearchShow( std::string& searchId /* in */ );
    bool savedSearchDestroy( std::string& searchId /* in */ );
    
    /* cURL APIs */
    bool isCurlInit();
    void getLastWebResponse( std::string& outWebResp /* out */ );
    void getLastCurlError( std::string& outErrResp /* out */);

    /* Internal cURL related methods */
    int saveLastWebResponse( char*& data, size_t size );
    static int curlCallback( char* data, size_t size, size_t nmemb, twitCurl* pTwitCurlObj );

    /* cURL proxy APIs */
    std::string& getProxyServerIp();
    std::string& getProxyServerPort();
    std::string& getProxyUserName();
    std::string& getProxyPassword();
    void setProxyServerIp( std::string& proxyServerIp /* in */ );
    void setProxyServerPort( std::string& proxyServerPort /* in */ );
    void setProxyUserName( std::string& proxyUserName /* in */ );
    void setProxyPassword( std::string& proxyPassword /* in */ );

private:
    /* cURL data */
    CURL* m_curlHandle;
    char m_errorBuffer[twitCurlDefaults::TWITCURL_DEFAULT_BUFFSIZE];
    std::string m_callbackData;

    /* cURL flags */
    bool m_curlProxyParamsSet;
    bool m_curlLoginParamsSet;
    bool m_curlCallbackParamsSet;

    /* cURL proxy data */
    std::string m_proxyServerIp;
    std::string m_proxyServerPort;
    std::string m_proxyUserName;
    std::string m_proxyPassword;

    /* Twitter data */
    std::string m_twitterUsername;
    std::string m_twitterPassword;

    /* OAuth data */
    oAuth m_oAuth;

    /* Private methods */
    void clearCurlCallbackBuffers();
    void prepareCurlProxy();
    void prepareCurlCallback();
    void prepareCurlUserPass();
    void prepareStandardParams();
    bool performGet( const std::string& getUrl );
    bool performGet( const std::string& getUrl, const std::string& oAuthHttpHeader );
    bool performDelete( const std::string& deleteUrl );
    bool performPost( const std::string& postUrl, std::string dataStr = "" );
	bool performPostJSON(const std::string& postUrl, std::string dataStr);
};


/* Private functions */
void utilMakeCurlParams( std::string& outStr, std::string& inParam1, std::string& inParam2 );
void utilMakeUrlForUser( std::string& outUrl, const std::string& baseUrl, std::string& userInfo, bool isUserId );

#endif // _TWITCURL_H_
