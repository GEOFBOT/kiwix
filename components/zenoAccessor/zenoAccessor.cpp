#include "xpcom-config.h"
#include "nsIGenericFactory.h"
#include "IZenoAccessor.h"
#include <stdio.h>
#include <stdlib.h>

#include "nsXPCOM.h"
#include "nsEmbedString.h"
#include "nsIURI.h"

#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsIProperties.h"
#include "nsDirectoryServiceDefs.h"

#include <zeno/file.h>
#include <zeno/article.h>

#include <string>

using namespace std;

class ZenoAccessor : public IZenoAccessor {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_IZENOACCESSOR
  
  ZenoAccessor();

private:
  ~ZenoAccessor();

protected:
  zeno::File* zenoFileHandler;
  zeno::size_type firstArticleOffset;
  zeno::size_type lastArticleOffset;
  zeno::size_type currentArticleOffset;
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(ZenoAccessor, IZenoAccessor)

/* Constructor */
ZenoAccessor::ZenoAccessor()
  : zenoFileHandler(NULL)
{}

/* Destructor */
ZenoAccessor::~ZenoAccessor() {
  if (this->zenoFileHandler != NULL) {
    delete this->zenoFileHandler;
  }
}

/* Load zeno file */
NS_IMETHODIMP ZenoAccessor::LoadFile(const char *path, nsACString &_retval) {
  try {    
    this->zenoFileHandler = new zeno::File(path);

    if (this->zenoFileHandler != NULL) {
      this->firstArticleOffset = this->zenoFileHandler->getNamespaceBeginOffset('0');
      this->lastArticleOffset = this->zenoFileHandler->getNamespaceEndOffset('0');
      this->currentArticleOffset = this->firstArticleOffset;
    }
  }
  catch(...) { }
}

/* Reset the cursor for GetNextArticle() */
NS_IMETHODIMP ZenoAccessor::Reset(PRBool *retVal) {
  this->currentArticleOffset = this->firstArticleOffset;
  *retVal = PR_TRUE;
}

/* List articles for a namespace */
NS_IMETHODIMP ZenoAccessor::GetNextArticle(char **url, char **content, PRBool *retVal) {
  try {
    zeno::Article currentArticle;
    
    /* get next non redirect article */
    do {
      currentArticle = this->zenoFileHandler->getArticle(this->currentArticleOffset);
    } while (currentArticle.getRedirectFlag() && 
	     this->currentArticleOffset != this->lastArticleOffset && 
	     this->currentArticleOffset++);
    
    /* returned values*/
    string urlStr = currentArticle.getUrl().getValue();
    *url = (char*) NS_Alloc(urlStr.length()+1);
    strcpy(*url, urlStr.c_str());

    string contentStr = currentArticle.getData();
    *content = (char*) NS_Alloc(contentStr.length()+1);
    strcpy(*content, contentStr.c_str());

    /* Set returned value */
    if (this->currentArticleOffset != this->lastArticleOffset) {
      this->currentArticleOffset++;
      *retVal = PR_TRUE;
    } else {
      this->currentArticleOffset = this->firstArticleOffset;
      *retVal = PR_FALSE;
    }
  }
  catch(...) { }
}

/* Get a content from a zeno file */
NS_IMETHODIMP ZenoAccessor::GetContent(nsIURI *urlObject, char **contentType, nsACString &_retval) {

  /* Convert the URL object to char* string */
  nsEmbedCString urlString;
  urlObject->GetPath(urlString);
  const char *url = urlString.get();
  
  /* Offset to visit the url */
  unsigned int urlLength = strlen(url);
  unsigned int offset = 0;

  /* Ignore the '/' */
  while((offset < urlLength) && (url[offset] == '/')) offset++;

  /* Get namespace */
  char ns[1024];
  unsigned int nsOffset = 0;
  while((offset < urlLength) && (url[offset] != '/')) {
    ns[nsOffset] = url[offset];
    offset++;
    nsOffset++;
  }
  ns[nsOffset] = 0;

  /* Ignore the '/' */
  while((offset < urlLength) && (url[offset] == '/')) offset++;  

  /* Get content title */
  char title[1024];
  unsigned int titleOffset = 0;
  while((offset < urlLength) && (url[offset] != '/')) {
    title[titleOffset] = url[offset];
    offset++;
    titleOffset++;
  }
  title[titleOffset] = 0;

  /* Extract the content from the zeno file */
  zeno::Article article = zenoFileHandler->getArticle(ns[0], zeno::QUnicodeString(title));
  
  /* redirection handling */
  unsigned int redirectionCount = 0;
  while (article.getRedirectFlag() && redirectionCount++ < 5) {
    article = article.getRedirectArticle();
  }

  /* Get the data length */
  unsigned int contentLength = article.getDataLen();

  /* Get the content mime-type */
  const char *mimeType = article.getMimeType().c_str();
  *contentType = (char*) NS_Alloc(strlen(mimeType) + 1);
  strcpy(*contentType, mimeType);

  /* Get the data */
  std::string contentString = article.getData();
  std::string::size_type contentStringSize = contentString.size();
  char *content = (char *) NS_Alloc(contentStringSize + 1);
  unsigned inc = 0;
  for(inc = 0; inc < contentStringSize; inc++) {
    content[inc] = contentString[inc];
  }
  _retval = nsDependentCString(content, contentStringSize);
  NS_Free(content);

  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(ZenoAccessor)

static const nsModuleComponentInfo components[] =
{
   { "zenoAccessor",
     IZENOACCESSOR_IID,
     "@kiwix.org/zenoAccessor",
     ZenoAccessorConstructor
   }
};

NS_IMPL_NSGETMODULE(nsZenoAccessor, components)
