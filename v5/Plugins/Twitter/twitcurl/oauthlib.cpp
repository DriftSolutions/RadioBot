#include "oauthlib.h"
#include "HMAC_SHA1.h"
#include "base64.h"
#include "urlencode.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*++
* @method: oAuth::oAuth
*
* @description: constructor
*
* @input: none
*
* @output: none
*
*--*/
oAuth::oAuth():
m_consumerKey( "" ),
m_consumerSecret( "" ),
m_oAuthTokenKey( "" ),
m_oAuthTokenSecret( "" ),
m_oAuthPin( "" ),
m_nonce( "" ),
m_timeStamp( "" ),
m_oAuthScreenName( "" )
{
}

/*++
* @method: oAuth::~oAuth
*
* @description: destructor
*
* @input: none
*
* @output: none
*
*--*/
oAuth::~oAuth()
{
}

/*++
* @method: oAuth::getConsumerKey
*
* @description: this method gives consumer key that is being used currently
*
* @input: none
*
* @output: consumer key
*
*--*/
void oAuth::getConsumerKey( std::string& consumerKey )
{
    consumerKey = m_consumerKey;
}

/*++
* @method: oAuth::setConsumerKey
*
* @description: this method saves consumer key that should be used
*
* @input: consumer key
*
* @output: none
*
*--*/
void oAuth::setConsumerKey( const std::string& consumerKey )
{
    m_consumerKey.assign( consumerKey.c_str() );
}

/*++
* @method: oAuth::getConsumerSecret
*
* @description: this method gives consumer secret that is being used currently
*
* @input: none
*
* @output: consumer secret
*
*--*/
void oAuth::getConsumerSecret( std::string& consumerSecret )
{
    consumerSecret = m_consumerSecret;
}

/*++
* @method: oAuth::setConsumerSecret
*
* @description: this method saves consumer secret that should be used
*
* @input: consumer secret
*
* @output: none
*
*--*/
void oAuth::setConsumerSecret( const std::string& consumerSecret )
{
    m_consumerSecret = consumerSecret;
}

/*++
* @method: oAuth::getOAuthTokenKey
*
* @description: this method gives OAuth token (also called access token) that is being used currently
*
* @input: none
*
* @output: OAuth token
*
*--*/
void oAuth::getOAuthTokenKey( std::string& oAuthTokenKey )
{
    oAuthTokenKey = m_oAuthTokenKey;
}

/*++
* @method: oAuth::setOAuthTokenKey
*
* @description: this method saves OAuth token that should be used
*
* @input: OAuth token
*
* @output: none
*
*--*/
void oAuth::setOAuthTokenKey( const std::string& oAuthTokenKey )
{
    m_oAuthTokenKey = oAuthTokenKey;
}

/*++
* @method: oAuth::getOAuthTokenSecret
*
* @description: this method gives OAuth token secret that is being used currently
*
* @input: none
*
* @output: OAuth token secret
*
*--*/
void oAuth::getOAuthTokenSecret( std::string& oAuthTokenSecret )
{
    oAuthTokenSecret = m_oAuthTokenSecret;
}

/*++
* @method: oAuth::setOAuthTokenSecret
*
* @description: this method saves OAuth token that should be used
*
* @input: OAuth token secret
*
* @output: none
*
*--*/
void oAuth::setOAuthTokenSecret( const std::string& oAuthTokenSecret )
{
    m_oAuthTokenSecret = oAuthTokenSecret;
}

/*++
* @method: oAuth::getOAuthScreenName
*
* @description: this method gives authorized user's screenname
*
* @input: none
*
* @output: screen name
*
*--*/
void oAuth::getOAuthScreenName( std::string& oAuthScreenName )
{
    oAuthScreenName = m_oAuthScreenName;
}

/*++
* @method: oAuth::setOAuthScreenName
*
* @description: this method sets authorized user's screenname
*
* @input: screen name
*
* @output: none
*
*--*/
void oAuth::setOAuthScreenName( const std::string& oAuthScreenName )
{
    m_oAuthScreenName = oAuthScreenName;
}

/*++
* @method: oAuth::getOAuthPin
*
* @description: this method gives OAuth verifier PIN
*
* @input: none
*
* @output: OAuth verifier PIN
*
*--*/
void oAuth::getOAuthPin( std::string& oAuthPin )
{
    oAuthPin = m_oAuthPin;
}

/*++
* @method: oAuth::setOAuthPin
*
* @description: this method sets OAuth verifier PIN
*
* @input: OAuth verifier PIN
*
* @output: none
*
*--*/
void oAuth::setOAuthPin( const std::string& oAuthPin )
{
    m_oAuthPin = oAuthPin;
}

/*++
* @method: oAuth::generateNonceTimeStamp
*
* @description: this method generates nonce and timestamp for OAuth header
*
* @input: none
*
* @output: none
*
* @remarks: internal method
*
*--*/
void oAuth::generateNonceTimeStamp()
{
    char szTime[oAuthLibDefaults::OAUTHLIB_BUFFSIZE];
    char szRand[oAuthLibDefaults::OAUTHLIB_BUFFSIZE];
    memset( szTime, 0, oAuthLibDefaults::OAUTHLIB_BUFFSIZE );
    memset( szRand, 0, oAuthLibDefaults::OAUTHLIB_BUFFSIZE );
    srand( time( NULL ) );
    sprintf( szRand, "%x", rand()%1000 );
    sprintf( szTime, "%ld", time( NULL ) );
    
    m_nonce.assign( szTime );
    m_nonce.append( szRand );
    m_timeStamp.assign( szTime );
}

/*++
* @method: oAuth::buildOAuthTokenKeyValuePairs
*
* @description: this method prepares key-value pairs required for OAuth header
*               and signature generation.
*
* @input: includeOAuthVerifierPin - flag to indicate whether oauth_verifer key-value
*                                   pair needs to be included. oauth_verifer is only
*                                   used during exchanging request token with access token.
*         rawData - url encoded data. this is used during signature generation.
*         oauthSignature - base64 and url encoded OAuth signature.
*
* @output: keyValueMap - map in which key-value pairs are populated
*
* @remarks: internal method
*
*--*/
bool oAuth::buildOAuthTokenKeyValuePairs( const bool includeOAuthVerifierPin,
                                          const std::string& rawData,
                                          const std::string& oauthSignature,
                                          oAuthKeyValuePairs& keyValueMap )
{
    /* Generate nonce and timestamp if required */
    generateNonceTimeStamp();

    /* Consumer key and its value */
    keyValueMap[oAuthLibDefaults::OAUTHLIB_CONSUMERKEY_KEY] = m_consumerKey;

    /* Nonce key and its value */
    keyValueMap[oAuthLibDefaults::OAUTHLIB_NONCE_KEY] = m_nonce;

    /* Signature if supplied */
    if( oauthSignature.length() > 0 )
    {
        keyValueMap[oAuthLibDefaults::OAUTHLIB_SIGNATURE_KEY] = oauthSignature;
    }

    /* Signature method, only HMAC-SHA1 as of now */
    keyValueMap[oAuthLibDefaults::OAUTHLIB_SIGNATUREMETHOD_KEY] = std::string( "HMAC-SHA1" );

    /* Timestamp */
    keyValueMap[oAuthLibDefaults::OAUTHLIB_TIMESTAMP_KEY] = m_timeStamp;

    /* Token */
    if( m_oAuthTokenKey.length() > 0 )
    {
        keyValueMap[oAuthLibDefaults::OAUTHLIB_TOKEN_KEY] = m_oAuthTokenKey;
    }

    /* Verifier */
    if( includeOAuthVerifierPin && ( m_oAuthPin.length() > 0 ) )
    {
        keyValueMap[oAuthLibDefaults::OAUTHLIB_VERIFIER_KEY] = m_oAuthPin;
    }

    /* Version */
    keyValueMap[oAuthLibDefaults::OAUTHLIB_VERSION_KEY] = std::string( "1.0" );

    /* Data if it's present */
    if( rawData.length() > 0 )
    {
        /* Data should already be urlencoded once */
        std::string dummyStrKey( "" );
        std::string dummyStrValue( "" );
        size_t nPos = rawData.find_first_of( "=" );
        if( std::string::npos != nPos )
        {
            dummyStrKey = rawData.substr( 0, nPos );
            dummyStrValue = rawData.substr( nPos + 1 );
            keyValueMap[dummyStrKey] = dummyStrValue;
        }
    }

    return ( keyValueMap.size() > 0 ) ? true : false;
}

/*++
* @method: oAuth::getSignature
*
* @description: this method calculates HMAC-SHA1 signature of OAuth header
*
* @input: eType - HTTP request type
*         rawUrl - raw url of the HTTP request
*         rawKeyValuePairs - key-value pairs containing OAuth headers and HTTP data
*
* @output: oAuthSignature - base64 and url encoded signature
*
* @remarks: internal method
*
*--*/
bool oAuth::getSignature( const eOAuthHttpRequestType eType,
                          const std::string& rawUrl,
                          const oAuthKeyValuePairs& rawKeyValuePairs,
                          std::string& oAuthSignature )
{
    std::string rawParams( "" );
    std::string paramsSeperator( "" );
    std::string sigBase( "" );

    /* Initially empty signature */
    oAuthSignature.assign( "" );

    /* Build a string using key-value pairs */
    paramsSeperator = "&";
    getStringFromOAuthKeyValuePairs( rawKeyValuePairs, rawParams, paramsSeperator );

    /* Start constructing base signature string. Refer http://dev.twitter.com/auth#intro */
    switch( eType )
    {
    case eOAuthHttpGet:
        {
            sigBase.assign( "GET&" );
        }
        break;

    case eOAuthHttpPost:
        {
            sigBase.assign( "POST&" );
        }
        break;

    case eOAuthHttpDelete:
        {
            sigBase.assign( "DELETE&" );
        }
        break;

    default:
        {
            return false;
        }
        break;
    }
    sigBase.append( urlencode( rawUrl ) );
    sigBase.append( "&" );
    sigBase.append( urlencode( rawParams ) );

    /* Now, hash the signature base string using HMAC_SHA1 class */
    CHMAC_SHA1 objHMACSHA1;
    std::string secretSigningKey( "" );
    unsigned char strDigest[oAuthLibDefaults::OAUTHLIB_BUFFSIZE_LARGE];

    memset( strDigest, 0, oAuthLibDefaults::OAUTHLIB_BUFFSIZE_LARGE );

    /* Signing key is composed of consumer_secret&token_secret */
    secretSigningKey.assign( m_consumerSecret.c_str() );
    secretSigningKey.append( "&" );
    if( m_oAuthTokenSecret.length() > 0 )
    {
        secretSigningKey.append( m_oAuthTokenSecret.c_str() );
    }
  
    objHMACSHA1.HMAC_SHA1( (unsigned char*)sigBase.c_str(),
                           sigBase.length(),
                           (unsigned char*)secretSigningKey.c_str(),
                           secretSigningKey.length(),
                           strDigest ); 

    /* Do a base64 encode of signature */
    std::string base64Str = base64_encode( strDigest, 20 /* SHA 1 digest is 160 bits */ );

    /* Do an url encode */
    oAuthSignature = urlencode( base64Str );

    return ( oAuthSignature.length() > 0 ) ? true : false;
}

/*++
* @method: oAuth::getOAuthHeader
*
* @description: this method builds OAuth header that should be used in HTTP requests to twitter
*
* @input: eType - HTTP request type
*         rawUrl - raw url of the HTTP request
*         rawData - HTTP data
*         includeOAuthVerifierPin - flag to indicate whether or not oauth_verifier needs to included
*                                   in OAuth header
*
* @output: oAuthHttpHeader - OAuth header
*
*--*/
bool oAuth::getOAuthHeader( const eOAuthHttpRequestType eType,
                            const std::string& rawUrl,
                            const std::string& rawData,
                            std::string& oAuthHttpHeader,
                            const bool includeOAuthVerifierPin )
{
    oAuthKeyValuePairs rawKeyValuePairs;
    std::string rawParams( "" );
    std::string oauthSignature( "" );
    std::string paramsSeperator( "" );
    std::string pureUrl( rawUrl );

    /* Clear header string initially */
    oAuthHttpHeader.assign( "" );
    rawKeyValuePairs.clear();

    /* If URL itself contains ?key=value, then extract and put them in map */
    size_t nPos = rawUrl.find_first_of( "?" );
    if( std::string::npos != nPos )
    {
        /* Get only URL */
        pureUrl = rawUrl.substr( 0, nPos );

        /* Get only key=value data part */
        std::string dataPart = rawUrl.substr( nPos + 1 );
        size_t nPos2 = dataPart.find_first_of( "=" );
        if( std::string::npos != nPos2 )
        {
            std::string dataKey = dataPart.substr( 0, nPos2 );
            std::string dataVal = dataPart.substr( nPos2 + 1 );

            /* Put this key=value pair in map */
            rawKeyValuePairs[dataKey] = urlencode( dataVal );
        }
    }

    /* Build key-value pairs needed for OAuth request token, without signature */
    buildOAuthTokenKeyValuePairs( includeOAuthVerifierPin, rawData, std::string( "" ), rawKeyValuePairs );

    /* Get url encoded base64 signature using request type, url and parameters */
    getSignature( eType, pureUrl, rawKeyValuePairs, oauthSignature );

    /* Now, again build key-value pairs with signature this time */
    buildOAuthTokenKeyValuePairs( includeOAuthVerifierPin, std::string( "" ), oauthSignature, rawKeyValuePairs );

    /* Get OAuth header in string format */
    paramsSeperator = ",";
    getStringFromOAuthKeyValuePairs( rawKeyValuePairs, rawParams, paramsSeperator );

    /* Build authorization header */
    oAuthHttpHeader.assign( oAuthLibDefaults::OAUTHLIB_AUTHHEADER_STRING.c_str() );
    oAuthHttpHeader.append( rawParams.c_str() );

    return ( oAuthHttpHeader.length() > 0 ) ? true : false;
}

/*++
* @method: oAuth::getStringFromOAuthKeyValuePairs
*
* @description: this method builds a sorted string from key-value pairs
*
* @input: rawParamMap - key-value pairs map
*         paramsSeperator - sepearator, either & or ,
*
* @output: rawParams - sorted string of OAuth parameters
*
* @remarks: internal method
*
*--*/
bool oAuth::getStringFromOAuthKeyValuePairs( const oAuthKeyValuePairs& rawParamMap,
                                             std::string& rawParams,
                                             const std::string& paramsSeperator )
{
    rawParams.assign( "" );
    if( rawParamMap.size() )
    {
        oAuthKeyValueList keyValueList;
        std::string dummyStr( "" );

        /* Push key-value pairs to a list of strings */
        keyValueList.clear();
        oAuthKeyValuePairs::const_iterator itMap = rawParamMap.begin();
        for( ; itMap != rawParamMap.end(); itMap++ )
        {
            dummyStr.assign( itMap->first );
            dummyStr.append( "=" );
            if( paramsSeperator == "," )
            {
                dummyStr.append( "\"" );
            }
            dummyStr.append( itMap->second );
            if( paramsSeperator == "," )
            {
                dummyStr.append( "\"" );
            }
            keyValueList.push_back( dummyStr );
        }

        /* Sort key-value pairs based on key name */
        keyValueList.sort();

        /* Now, form a string */
        dummyStr.assign( "" );
        oAuthKeyValueList::iterator itKeyValue = keyValueList.begin();
        for( ; itKeyValue != keyValueList.end(); itKeyValue++ )
        {
            if( dummyStr.length() > 0 )
            {
                dummyStr.append( paramsSeperator );
            }
            dummyStr.append( itKeyValue->c_str() );
        }
        rawParams.assign( dummyStr );
    }
    return ( rawParams.length() > 0 ) ? true : false;
}

/*++
* @method: oAuth::extractOAuthTokenKeySecret
*
* @description: this method extracts oauth token key and secret from
*               twitter's HTTP response
*
* @input: requestTokenResponse - response from twitter
*
* @output: none
*
*--*/
bool oAuth::extractOAuthTokenKeySecret( const std::string& requestTokenResponse )
{
    if( requestTokenResponse.length() > 0 )
    {
        size_t nPos = std::string::npos;
        std::string strDummy( "" );

        /* Get oauth_token key */
        nPos = requestTokenResponse.find( oAuthLibDefaults::OAUTHLIB_TOKEN_KEY.c_str() );
        if( std::string::npos != nPos )
        {
            nPos = nPos + oAuthLibDefaults::OAUTHLIB_TOKEN_KEY.length() + strlen( "=" );
            strDummy = requestTokenResponse.substr( nPos );
            nPos = strDummy.find( "&" );
            if( std::string::npos != nPos )
            {
                m_oAuthTokenKey = strDummy.substr( 0, nPos );
            }
        }

        /* Get oauth_token_secret */
        nPos = requestTokenResponse.find( oAuthLibDefaults::OAUTHLIB_TOKENSECRET_KEY.c_str() );
        if( std::string::npos != nPos )
        {
            nPos = nPos + oAuthLibDefaults::OAUTHLIB_TOKENSECRET_KEY.length() + strlen( "=" );
            strDummy = requestTokenResponse.substr( nPos );
            nPos = strDummy.find( "&" );
            if( std::string::npos != nPos )
            {
                m_oAuthTokenSecret = strDummy.substr( 0, nPos );
            }
        }

        /* Get screen_name */
        nPos = requestTokenResponse.find( oAuthLibDefaults::OAUTHLIB_SCREENNAME_KEY.c_str() );
        if( std::string::npos != nPos )
        {
            nPos = nPos + oAuthLibDefaults::OAUTHLIB_SCREENNAME_KEY.length() + strlen( "=" );
            strDummy = requestTokenResponse.substr( nPos );
            m_oAuthScreenName = strDummy;
        }
    }
    return true;
}
