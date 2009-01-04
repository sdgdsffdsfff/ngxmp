/**-------------------------/// nginx sdb protocal parser \\\---------------------------
 *
 * <b>sdb protocal parser</b>
 * @version : 1.0
 * @since : 2008 10 20 03:03:27 PM
 * 
 * @description :
 *   
 * @usage : 
 * 
 * @author : drdr.xp | drdr.xp@gmail.com
 * @copyright sina.com.cn 
 * @TODO : 
 *
 *--------------------------\\\ nginx sdb protocal parser ///---------------------------*/



/**
 * responses
 */
#define SDB_CODE_SUCCESS       "SUCCESS"
#define SDB_CODE_CLIENT_ERROR  "CLIENT_ERROR"
#define SDB_CODE_SERVER_ERROR  "SERVER_ERROR"



#define WS              "\\b"

#define BoxUsage        "([^ ]+)"
#define Code            "([^ ]+)"
#define Message         "([^ ]+)?"
#define CRLF            "\r\n"
#define AccessKeyId     "([^ ]+)"
#define DomainName      "([^ ]+)"
#define ItemName        "([^ ]+)"
#define ItemNames       "( [^ ]+)*"
#define AttributeName   "([^ ]+)"
#define Size            "([\\d]+)"
#define Bytes           "([\\d]+)"

/* RET <BoxUsage> <CODE> [Message]\r\n */
#define SDB_RET  "^RET "BoxUsage" "Code" ?"Message CRLF

/* ATTR <AccessKeyId> <DomainName> <ItemName> <AttributeName> <size>\r\n */
#define SDB_ATTR "^ATTR "AccessKeyId" "DomainName" "ItemName" "AttributeName" "Size CRLF

/* ATTRV <AttributeName> <bytes>\r\n */
#define SDB_ATTRV "^ATTRV "AttributeName" "Bytes CRLF


/* VALUE <DomainName> <bytes> [itemnames]*\r\n */
#define SDB_Q_VAL "^VALUE "DomainName" "Bytes ItemNames



/**
 * request /uri
 */

/**
 * https://sdb.amazonaws.com/
 * ?Action=GetAttributes
 * &AWSAccessKeyId=[valid access key id]
 * &DomainName=MyDomain
 * &ItemName=Item123
 * &AttributeName.0=Color
 * &AttributeName.1=Size
 * &SignatureVersion=1
 * &Timestamp=2007-06-25T15%3A03%3A07-07%3A00
 * &Version=2007-11-07
 * &Signature=IJQwarD3Zt0fCj8WLaNmgTU7jGw%3D
 */

#define rg_DomainName    WS"DomainName="DomainName
#define rg_ItemName      WS"ItemName="ItemName 
#define rg_AttributeName WS"AttributeName\\."Size"="AttributeName 


