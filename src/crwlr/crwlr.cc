#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <curl/curl.h>
#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include <libxml/HTMLparser.h>
#include <libxml/uri.h>
#include <math.h>
#include <libxml/xmlwriter.h>

#define DBG_NONE      0
#define DBG_PROCESS   1
#define DBG_STATS     2
#define DBG_XMLERROR  3
#define DBG_HINT      4
#define DBG_XML       5
#define DBG_MGET      6

#define DBG_BREAK  __asm__ __volatile__ ("int $0x03\n")

using namespace std;

class URL {
private:
    xmlURI  *_uri;
    const char*_proto;
    const char*_host;
    uint16_t  _port;
    const char*_path;
    const char *_full;
public:
    URL(const string& surl);
    ~URL();
    xmlURI *get_uri() {return _uri;};
    const char *get_proto() { return _proto;} ;
    void set_proto(const char *proto) {_proto = strdup(proto);}
    const char *get_host() { return _host;} ;
    void set_host(const char *host) {_host = host?strdup(host):NULL;}
    const char *get_path() { return _path;} ;
    uint16_t  get_port() { return _port;};
    void set_port(uint16_t port) { _port =  port; } ;
    void dump();
    bool operator==(URL&ourl);
    operator const char*() {return _full;}; 
    void calc_full();
    
private:
    void parse(const string &surl);
};

URL::URL(const string&surl)
{
  _full = NULL;
  _port = 80;
  parse(surl);
  calc_full();
}
URL::~URL()
{ 
  if(_uri) 
    xmlFreeURI(_uri),_uri = NULL;
  if(_full)
    free((void*)_full),_full = NULL;
  if(_proto)
    free((void*)_proto),_proto = NULL;
  if(_host)
    free((void*)_host),_host = NULL;
  if(_path)
    free((void*)_path),_path = NULL;
}
void URL::calc_full()
{
  ssize_t ret;
  if(_full)
    free((void*)_full),_full=NULL;
  if(_host){
    if((80!=_port) && (443!=_port))
      ret = snprintf(NULL,0,"%s://%s:%d/%s",_proto,_host,_port,_path);
    else
      ret = snprintf(NULL,0,"%s://%s/%s",_proto,_host,_path);
    if(ret>0) {
      ret ++;
      _full = (const char*)calloc(ret,sizeof(char));
      if((80!=_port) && (443!=_port))
        ret = snprintf((char*)_full,ret,"%s://%s:%d/%s",_proto,_host,_port,_path);
      else
        ret = snprintf((char*)_full,ret,"%s://%s/%s",_proto,_host,_path);
    }
  } else {
    _full = strdup(_path?_path:"/");  
  }
}

#define min(x,y) (((x)<(y))?(x):(y))

#define COMP_PROTO  0
#define COMP_HOST   1
#define COMP_PORT   2
#define COMP_PATH   3  

void URL::parse(const string &url)
{
  _host = _path = _proto = NULL;
  _uri = xmlParseURI(url.c_str());
  if(!_uri)
    printf("Failed to parse URI:%s\n",url.c_str());
  else {
  //  printf("Parse URI:%s\n",url.c_str());
    if(_uri->scheme)
      _proto = strdup(_uri->scheme);
    else
      _proto = strdup("http");
    if(_uri->server)
      _host = strdup(_uri->server);
    _port = 80;
    if(_uri->port)
      _port = _uri->port;
    if(_uri->path){
      if(*(_uri->path)!='/')
        _path = strdup(_uri->path);
      else
        _path = strdup(_uri->path+1);
    }else
      _path = strdup("");
  }
}
void URL::dump()
{
    printf("scheme: %s\n",get_proto());
    printf("server: %s\n",get_host());
    printf("port  : %d\n",get_port());
    printf("path  : %s\n",get_path());
}
bool URL::operator==(URL&ourl)
{
  if(_proto) {
    if(!ourl.get_proto())
      return false;
    if(strcmp(_proto,ourl.get_proto()))
      return false;
  }
  if(_host) {
    if(!ourl.get_host())
      return false;
    if(strcmp(_host,ourl.get_host()))
      return false;
  }
  if(_port) {
    if(_port != ourl.get_port())
      return false;
  }
  if(_path) {
    if(!ourl.get_path())
      return false;
    if(strcmp(_path,ourl.get_path()))
      return false;
  }
  return true;
}


class node_t{
private:
  const char *  _url;
  list<double>  _rt_list;
  list<string>  _out_links;
  double        _mean;
  double        _sdev;
public:
typedef list<string>::iterator link_iterator;
  void add_link(string &str) {_out_links.push_back(str);};
  void add_speed(double speed) {_rt_list.push_back(speed);};
  link_iterator start_links() {return _out_links.begin();};
  link_iterator end_links() {return _out_links.end();};
  void calc_stats();
  int get_num_probes() {return _rt_list.size();};
  double get_mean()const{return _mean;} ;
  void set_mean(double mean) {_mean = mean;};
  double get_sdev()const {return _sdev;};
  void set_sdev(double sdev) {_sdev = sdev;};
  void set_url(const char *url) {_url = url?strdup(url):NULL;};
  const char *get_url(){return _url;};
  bool operator <(const node_t& o) { return _mean<o.get_mean();};
  bool operator <=(const node_t& o) { return _mean<=o.get_mean();};
  bool operator >(const node_t& o) { return _mean>o.get_mean();};
  bool operator >=(const node_t& o) { return _mean>=o.get_mean();};
  node_t(node_t*p);
  node_t();
  ~node_t();
  
};

node_t::node_t()
{
  _url = NULL;
  _mean = 0;
  _sdev = 0;
}
node_t::node_t(node_t*p)
{
  _mean = p->get_mean();
  _sdev = p->get_sdev();
  _url = strdup(p->get_url());
}
node_t::~node_t()
{
  if(_url)
    free((void*)_url),_url = NULL; 
}

bool slower(node_t *n1,  node_t *n2)
{
  return (*n1)<(*n2);
}

bool longer(node_t *n1,  node_t *n2)
{
  return (*n1)>(*n2);
}

void node_t::calc_stats()
{
  _mean = 0;
  _sdev = 0;
  if(!_rt_list.size())
    return;
  for(list<double>::iterator it=_rt_list.begin();it!=_rt_list.end();it++) {
    //printf("%-13f ",*it);
    _mean +=*it;
  }
  _mean /=_rt_list.size();
  //printf("mean: %-13f\n",_mean);
  for(list<double>::iterator it=_rt_list.begin();it!=_rt_list.end();it++) {
    _sdev += (*it - _mean)*(*it - _mean);
  }
  _sdev /= _rt_list.size();
  _sdev = sqrt(_sdev);
}

typedef struct{
  CURL  *hcurl;
  char  *url;
  int depth;
  int count;
  int debug;
  char *dot_fname;
  char *xml_fname;
  char *in_fname;
  FILE *hdot;
#define SM_SPEED  0
#define SM_TIME   1
  int  sort_mode;
  xmlTextWriterPtr xml_writer;
  map<string,node_t* > site_map;
} context_t; 

#define OPT_INVALID   0
#define OPT_URL       1
#define OPT_DEPTH     2
#define OPT_COUNT     3
#define OPT_DOT       4
#define OPT_VERBOSE   5
#define OPT_XMLFILE   6
#define OPT_INFILE    7
#define OPT_HELP      8
#define OPT_SORTMODE  9
struct _help_content_t{
  int param_id;
  const char *legend;
} help_content[] = {
  { OPT_URL,    "the starting URL for the crawl phase."},
  { OPT_DEPTH,  "how many levels of links the crawler should follow."},
  { OPT_COUNT,  "how many probes to perform to collect for each page. \n\tSpecify at least 10 to get better results on mean calculation."},
  { OPT_DOT,    "filename to save graphviz::dot language representation of the web site."},
  { OPT_VERBOSE,"Turn on verbose output. Used for troubleshooting mostly. Valid numbers are 1-10"},
  { OPT_XMLFILE,"XML filename to save the web site crawl stats."},
  { OPT_INFILE,  "XML filename to read the web site crawl stats.\n\tThis instructs the tool to skip crawling and proceed to stress testing phase."},
  { OPT_HELP,    "This info."},
  { OPT_SORTMODE,"Sort URLs by either download speed(slowest to fastest) or\n\t request to response time (longest to shortest)."},
  {0, NULL}
};


struct option options[] = {
  {"url",     1 ,  NULL,  OPT_URL },
  {"depth",   1 ,  NULL,  OPT_DEPTH },
  {"count",   1 ,  NULL,  OPT_COUNT },
  {"dot",     1 ,  NULL,  OPT_DOT },
  {"verbose", 1 ,  NULL,  OPT_VERBOSE },
  {"xml",     1 ,  NULL,  OPT_XMLFILE },
  {"in",      1 ,  NULL,  OPT_INFILE },
  {"help",    0 ,  NULL,  OPT_HELP },
  {"sort",    1 ,  NULL,  OPT_SORTMODE },

  {NULL  ,    0  ,  NULL,  0  }
};

static void usage()
{
  printf("Usage\n");
  for(int i=0;help_content[i].legend;i++){
    printf("--%s\n\t%s\n",options[i].name,help_content[i].legend);
  }  
}
static int parse_opts(int argc,char*const argv[],context_t *ctx)
{

  extern char *optarg;
  int ind=0,ret;
  while(1){
    ret = getopt_long(argc,argv,"",options,&ind);
    if(-1==ind || -1==ret)
      break;
    switch(ret){
      case OPT_URL:
          ctx->url = strdup(optarg);
        break;
      case OPT_DEPTH:
          ctx->depth = atoi(optarg);
        break;
      case OPT_COUNT:
          ctx->count = atoi(optarg);
        break;
      case OPT_DOT:
          ctx->dot_fname = strdup(optarg);
        break;
      case OPT_VERBOSE:
          ctx->debug = atoi(optarg);
        break;
      case OPT_XMLFILE:
          ctx->xml_fname = strdup(optarg);
        break;
      case OPT_INFILE:
          ctx->in_fname = strdup(optarg);
        break;
      case OPT_SORTMODE:
        if(optarg && !strcasecmp(optarg,"speed"))
          ctx->sort_mode = SM_SPEED;
        else if(optarg && !strcasecmp(optarg,"time"))
          ctx->sort_mode = SM_TIME;
        else{
          printf("Unrecognized sort mode %s\n",argv[ind]);
          usage();
          exit(-1);
        }
        break;
      case '?':
        if(ind)
          printf("Unknown option %s\n",argv[ind]);
          //fallthrough
      default:
        usage();
        exit(-1);
    }
  }
  
}


typedef struct {
  URL *uri;
  list<string> *  url_list;
  context_t *global_ctx;
  int cur_depth;
} sax_ctx_t;

static void  htmlstart_el(void * ctx, 
           const xmlChar * name, 
           const xmlChar ** atts)
{  
  sax_ctx_t *sax_ctx=(sax_ctx_t*)ctx;
  int indent = sax_ctx->global_ctx->depth - sax_ctx->cur_depth;
//printf("%s: processing start tag %s\n",__FUNCTION__,(const char *)name);
  if(!strcmp("a",(const char*)name) && atts){
    for(int i=0;atts[i];i+=2){
      if(!strcmp((const char*)atts[i],"href")){
        char *ptr=NULL;
        char *url_buf=NULL;
        if(sax_ctx->global_ctx->debug>=DBG_XML)
          printf("%*sFound a link:%s\n",indent,"",atts[i+1]);
        if('\0'==*atts[i+1])
          continue;
        if('#'==*atts[i+1])
          continue;
        url_buf = strdup((const char*)atts[i+1]);
        ptr = ::strchr(url_buf,(int)'?');
        if(ptr){
          *ptr = '\0';
        }
        /*if(!ptr || !strncmp(ptr,".htm",4) || !strncmp(ptr,".html",5) || !strncmp(ptr,".php",4))*/{
          URL cur_url(url_buf);
          bool ins = false;
          const char *tmp = cur_url.get_host();
          if(!cur_url.get_uri())
            continue;
          if(sax_ctx->global_ctx->debug>=DBG_XML){
            printf("%*scur_url host:%s\n",indent,"",(const char *)tmp);
            printf("%*ssax_url host:%s\n",indent,"",(const char *)sax_ctx->uri->get_host());
          }
          if(tmp){
            if(sax_ctx->uri->get_host() && !strcmp(sax_ctx->uri->get_host(),tmp)){
              ins = true;
            }
          }else{
            ins = true;
            cur_url.set_host(sax_ctx->uri->get_host());
            cur_url.calc_full();
          }
          if(ins) {
            if(cur_url == (*sax_ctx->uri))
              continue;
            //printf("inserting  url host:%s\n",(const char *)cur_url);
            //string *str = new string((const char *)atts[i+1]);
            sax_ctx->url_list->push_back(string((const char *)cur_url));  
          }
        }
        if(url_buf)
          free(url_buf),url_buf = NULL;
      }
    }
  }
}
static void  htmlerror(void * ctx, 
           const char * msg, 
           ...)
{
  va_list va;
  va_start(va,msg);
  vprintf(msg,va);
  va_end(va);
}

static int process_page(context_t *ctx,URL &uri,const char *filename,list<string>* url_list,int cur_depth)
{
  sax_ctx_t sax_ctx={0};
  htmlSAXHandler sax={0};
  //uri.dump();
  sax_ctx.url_list = url_list;
  sax_ctx.uri = &uri;
  sax_ctx.global_ctx = ctx;
  sax_ctx.cur_depth = cur_depth;
  sax.startElement = htmlstart_el;
  if(ctx->debug >= DBG_XMLERROR){
    sax.error = htmlerror;
    sax.fatalError = htmlerror;
    sax.warning = htmlerror;
  }

  htmlHandleOmittedElem(1);  
  //htmlAutoCloseTag(1);
  if(!htmlSAXParseFile(filename,NULL,&sax,&sax_ctx)){
    //printf("Failed to parse page[%s].\n",filename);
  }
}

bool is2xx( long code) 
{
    return code >= 200 && code < 300;  
}

#define TMP_FNAME  "./tmp"
static int get_url(context_t *ctx,const char *url,int method,int depth)
{
  CURLcode ret;
  FILE *hfile = NULL;
  double speed=0;
  node_t *node=NULL;
    long   resp_code = 0;
  URL uri(url);
  char *eff_uri = NULL;

  list<string> url_list;

  node = ctx->site_map[((const char*)uri)];
  if(!node){
    hfile = fopen(TMP_FNAME,"w+");
    if(!hfile){
      printf("Error opening HTML temp file[%m]\n");
      return -1;
    }
    ret = curl_easy_setopt(ctx->hcurl, CURLOPT_WRITEDATA,hfile);
  } else {
    hfile = fopen("/dev/null","w+");
    if(!hfile){
      printf("Error opening /dev/null[%m]\n");
      return -1;
    }
    ret = curl_easy_setopt(ctx->hcurl, CURLOPT_WRITEDATA,hfile);
  }
//    ret = curl_easy_setopt(hcurl, CURLOPT_VERBOSE, 1);
  ret = curl_easy_setopt(ctx->hcurl, CURLOPT_URL,(const char*)uri);

  ret = curl_easy_perform(ctx->hcurl);
  if(hfile)
    fclose(hfile),hfile = NULL;
  if(ret){
    goto exit;
    //handle the error
  }
  if(SM_SPEED == ctx->sort_mode){
    ret = curl_easy_getinfo(ctx->hcurl,CURLINFO_SPEED_DOWNLOAD ,&speed);
    speed /=1024; 
  }else if(SM_TIME == ctx->sort_mode)
    ret = curl_easy_getinfo(ctx->hcurl,CURLINFO_STARTTRANSFER_TIME ,&speed);
  ret = curl_easy_getinfo(ctx->hcurl,CURLINFO_EFFECTIVE_URL,&eff_uri);
  ret = curl_easy_getinfo(ctx->hcurl,CURLINFO_RESPONSE_CODE, &resp_code);

  if(!is2xx(resp_code)) {
    printf("%*s[%d]skipping URL:%s %ld\n",ctx->depth - depth,"",depth,(const char *)eff_uri,resp_code);
    goto exit;
  }

  if(ctx->debug >=DBG_PROCESS) 
    printf("%*s[%d]processing URL:%s[%ld] %f\n",ctx->depth - depth,"",depth,(const char *)eff_uri,resp_code,speed);
  if(!node){
    node = new node_t;
    //ctx->site_map[((const char*)uri)] = node;
    ctx->site_map[string(eff_uri)] = node;
    process_page(ctx,uri,TMP_FNAME,&url_list,depth);
    for(list<string>::iterator it = url_list.begin();it!=url_list.end();it++){
      node->add_link(*it);
    }
    if(depth>0) {
      for(list<string>::iterator it = url_list.begin();it!=url_list.end();it++){
        node_t *tmp_node =ctx->site_map[*it];
        if(!tmp_node){
          if(ctx->debug >=DBG_PROCESS) 
            printf("%*s[%d]Accepting '%s' into the map.\n",ctx->depth - depth,"",depth,it->c_str());
          get_url(ctx,it->c_str(),method,depth-1);
        }
        else{
          if(ctx->debug >=DBG_PROCESS) 
            printf("%*s[%d]Rejecting '%s' already in the map.\n",ctx->depth - depth,"",depth,it->c_str());
        }
      }
    }
  } else {
    node->add_speed(speed);
  }

exit:
  ;
}

static int get_murl(context_t *ctx,vector<node_t*>& urls)
{
  CURLM *mhcurl=NULL;
  CURL *tmp_curl = NULL;
  int i=0,ret=0,handles=0,maxfd=0,j; 
  FILE *hfile=NULL;
  fd_set r_fds;  
  fd_set w_fds;  
  fd_set e_fds;  
  struct timeval tm;
  double speed;
  node_t *node=NULL;

  mhcurl = curl_multi_init();
  if(!mhcurl)
    return -1;
  hfile = fopen("/dev/null","w+");
  if(!hfile){
    printf("Error opening /dev/null[%m]\n");
    return -1;
  }
  for(i=0;i<urls.size();i++){
    node = urls[i];
    // set SHARE to share DNS cache
    tmp_curl = curl_easy_init();
    if(!tmp_curl)
      return -1;
    ret = curl_easy_setopt(tmp_curl, CURLOPT_WRITEDATA,hfile);

    //ret = curl_easy_setopt(hcurl, CURLOPT_VERBOSE, 1);
    ret = curl_easy_setopt(tmp_curl, CURLOPT_URL,node->get_url());
    if(ctx->debug>DBG_MGET)
      printf("get_murl:setting up curl_easy for %s\n",node->get_url());
    curl_multi_add_handle(mhcurl,tmp_curl);
  }
  do {
    ret = curl_multi_perform(mhcurl,&handles);
    
    if((ret == CURLM_OK) && !handles)
      break;
    if((ret == CURLM_OK) || (CURLM_CALL_MULTI_PERFORM == ret)){
      maxfd = 0;
      FD_ZERO(&r_fds);
      FD_ZERO(&w_fds);
      FD_ZERO(&e_fds);
      curl_multi_fdset(mhcurl,&r_fds,&w_fds,&e_fds,&maxfd);  
      if(ret != CURLM_OK)
        printf("get_murl: CURLM error %s\n",curl_multi_strerror((CURLMcode)ret));
      tm.tv_sec = 30;
      tm.tv_usec = 0;
      for(j=0;j<maxfd+1;j++)
        if(FD_ISSET(j,&w_fds))
          break;
      ret = select(maxfd+1,&r_fds,(j<(maxfd+1))?&w_fds:NULL,NULL,&tm);
      switch(ret){
        case -1:
          handles = 0;
          break;
        case 0:
          handles = 0;
          break;
      }
    }
  }while(handles);
  for(i=0;i<urls.size();i++){
    node = urls[i];
    CURLMsg *msg= NULL;
    int num_msg = 0;
    msg = curl_multi_info_read(mhcurl,&num_msg);
    if(!msg)
      break;
    tmp_curl = msg->easy_handle;
    if(CURLMSG_DONE == msg->msg) {
      char *eff_uri=NULL;
      curl_multi_remove_handle(mhcurl,tmp_curl);
      ret = curl_easy_getinfo(tmp_curl,CURLINFO_EFFECTIVE_URL,&eff_uri);
      if(!eff_uri)
        continue;
      node = ctx->site_map[string(eff_uri)];
      if(!node)
        continue;
      if(SM_SPEED == ctx->sort_mode){
        ret = curl_easy_getinfo(tmp_curl,CURLINFO_SPEED_DOWNLOAD ,&speed);
        speed /=1024;
      }else
        ret = curl_easy_getinfo(tmp_curl,CURLINFO_STARTTRANSFER_TIME,&speed);
      if(node){
        if(ctx->debug>DBG_MGET)
          printf("get_murl:adding speed[%lf] for%s eff_uri:%s\n",speed/1024,node->get_url(),eff_uri);
        node->add_speed(speed);
      }
    }
    curl_easy_cleanup(tmp_curl);
  }
  curl_multi_cleanup(mhcurl);
  fclose(hfile);
}

static int parse_in_xml(context_t* ctx,vector<node_t*>&urls)
{
  xmlDocPtr doc = NULL;
  xmlNodePtr xml_node = NULL,cur_node=NULL;
  if(!ctx->in_fname)
    return 0;
  doc = xmlParseFile(ctx->in_fname);
  xml_node = xmlDocGetRootElement(doc);
  if (xml_node->type == XML_ELEMENT_NODE) {
    if(xmlStrcmp(xml_node->name,(xmlChar*)"site")){
      if (ctx->debug>=DBG_HINT)
        printf("Failed to parse input XML. Failed to find urls root element\n");
      return 0;
    }
  }
  for(cur_node = xml_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(!xmlStrcmp(cur_node->name,(xmlChar*)"url")){
        node_t * url_node = new node_t;
        const char *url_str = (const char*)xmlNodeGetContent(cur_node);
        if(!url_str)
          continue;
        if('\n'==*url_str)
          url_str++;
        url_node->set_url(url_str);
        xmlChar *mean_attr = xmlGetProp(cur_node,(xmlChar*)"mean");
        xmlChar *sdev_attr = xmlGetProp(cur_node,(xmlChar*)"sdev");
        double mean=0;
        double sdev=0;
        if(mean_attr)
          mean = strtod((const char *)mean_attr,NULL);

        if(sdev_attr)
          sdev = strtod((const char *)sdev_attr,NULL);
        url_node->set_mean(mean);
        url_node->set_sdev(sdev);
        urls.push_back(url_node);
      }
    }
  }
  xmlFreeDoc(doc);
  return urls.size();
}

int main(int argc, char*const argv[])
{
  FILE *hdot = NULL;
  vector<node_t*> sorted_urls,in_urls,target_urls;
  context_t ctx={0};
  int i;

  ctx.url = NULL;  
  ctx.hcurl = curl_easy_init();
  ctx.depth = 1;
  ctx.debug = 0;

  parse_opts(argc,argv,&ctx);
  if(!ctx.url && !ctx.in_fname){
    printf("Both url and in options are missing.\n");
    usage();
    exit(-2);
  }
  if(ctx.xml_fname)
    ctx.xml_writer = xmlNewTextWriterFilename(ctx.xml_fname,0);
  if(ctx.debug)
    printf("starting URL=%s\n",ctx.url);
  if(ctx.dot_fname){
    ctx.hdot = fopen(ctx.dot_fname,"w+");
    fprintf(ctx.hdot,"digraph \"%s\" {\n\t layout=\"dot\";\n",ctx.url);
  }
  if(ctx.in_fname){
    if(parse_in_xml(&ctx,in_urls)){  
      if(ctx.debug>=DBG_HINT)
        printf("Parsed %ld urls\n",(long int)in_urls.size());
    }
  }

  if(curl_global_init(CURL_GLOBAL_ALL)) {
    printf("CURL initialization failed\n");
    exit(-3);
  }
  if(in_urls.size()) {
    ctx.depth = 0;
    double min_sdev=0;
    double max_sdev=0;
    double sdev_range=0;
    double sdev_cutoff=0;
    double sdev_cutoff_percent=0;
    int tgt_count =0;
    map<string,node_t *> orig_stats;
    for(vector<node_t*>::iterator it=in_urls.begin();it!=in_urls.end();it++){
      ctx.site_map[(*it)->get_url()] = *it;
      orig_stats[(*it)->get_url()] = new node_t(*it);
      if(ctx.debug>=DBG_HINT)
        printf("Working with hint url %s %lf/%lf\n",(*it)->get_url(),(*it)->get_mean(),(*it)->get_sdev());  
      if(it == in_urls.begin()){
        min_sdev = (*it)->get_sdev();
        max_sdev = (*it)->get_sdev();
      }
      if(min_sdev > (*it)->get_sdev())
        min_sdev = (*it)->get_sdev();
      if(max_sdev < (*it)->get_sdev())
        max_sdev = (*it)->get_sdev();
    }
    if(ctx.debug>=DBG_HINT)
      printf("SDEV range is [%lf, %lf]\n",floor(min_sdev),ceil(max_sdev));
    if(ctx.debug>=DBG_HINT)
      printf("SPEED AVG range is [%lf, %lf]\n",floor(in_urls[0]->get_mean()),ceil(in_urls[in_urls.size()-1]->get_mean()));
    sdev_range = max_sdev - min_sdev; 
    if(sdev_range) {
      tgt_count = 0;
      for(sdev_cutoff_percent=0.1;sdev_cutoff<1;sdev_cutoff+=0.1){
        sdev_cutoff = min_sdev + sdev_cutoff_percent * sdev_range;
        for(vector<node_t*>::iterator it=in_urls.begin();it!=in_urls.end();it++)
          if((*it)->get_sdev()<sdev_cutoff)
            tgt_count++;
        if(tgt_count>0){
          if(ctx.debug>=DBG_HINT)
            printf("Found %d target URLs with %3.f%% cutoff.\n",tgt_count,100*sdev_cutoff_percent);
          break;
        }
      }
    } else {
      tgt_count = in_urls.size();  
    }
    target_urls.resize(tgt_count);
    copy(in_urls.begin(),in_urls.begin()+tgt_count,target_urls.begin());
    for(i=0;i<ctx.count;i++){
      get_murl(&ctx,target_urls);
    }
/*
    for(;ctx.count>0;ctx.count--){
      for(int i=0;i<tgt_count;i++){
        node_t *tmp = in_urls[i];
        if(ctx.debug>=DBG_HINT)
          printf("get_url for %s with mean/sdev of %lf/%lf\n",tmp->get_url(),tmp->get_mean(),tmp->get_sdev());
        get_url(&ctx,tmp->get_url(),0,0);
      }
      //printf("\n");
    }
*/
    for(int i=0;i<tgt_count;i++){
      node_t *tmp = in_urls[i];
      node_t *orig = orig_stats[tmp->get_url()];
      tmp->calc_stats();
      printf("%s  original mean/sdev: %.3lf/%.3lf stress mean/sdev: %.3lf/%.3lf\n",
        tmp->get_url(),
        orig->get_mean(),
        orig->get_sdev(),
        tmp->get_mean(),
        tmp->get_sdev()
        );
    }
/*
    for(vector<node_t*>::iterator it=in_urls.begin();it != in_urls.end();it++) {
      if(*it)
        delete (*it);
    }
*/
    for(map<string,node_t *>::iterator it=orig_stats.begin();it!=orig_stats.end();it++){
      if(it->second)
        delete it->second;
    }

    goto cleanup;
  }
  get_url(&ctx,ctx.url,0,ctx.depth);
  ctx.depth = 0;
  if(ctx.debug)
    printf("Done with discovery.Discovered %ld urls.\n",(long int)ctx.site_map.size());
  for(int i=0;i<ctx.count;i++){
    for(map<string,node_t* >::iterator it = ctx.site_map.begin();it!=ctx.site_map.end();it++){
      get_url(&ctx,it->first.c_str(),0,0);
    }
  }
  for(map<string,node_t* >::iterator it = ctx.site_map.begin();it!=ctx.site_map.end();it++){
    if(!it->second)
      continue;
    it->second->calc_stats();
    it->second->set_url(it->first.c_str());
    sorted_urls.push_back(it->second);  
  }

  if(SM_SPEED == ctx.sort_mode)
    sort(sorted_urls.begin(),sorted_urls.end(),slower);
  else
    sort(sorted_urls.begin(),sorted_urls.end(),longer);
  
  if(ctx.xml_writer){
    xmlTextWriterStartDocument(ctx.xml_writer,NULL,NULL,NULL);
    xmlTextWriterStartElement(ctx.xml_writer,(xmlChar*)"site");
    xmlTextWriterWriteString(ctx.xml_writer,(xmlChar*)"\n");
  }
  for(vector<node_t*>::iterator it=sorted_urls.begin();it!=sorted_urls.end();it++){
    node_t *node = *it;
  //  if(ctx.debug>=DBG_STATS)
    if(SM_SPEED == ctx.sort_mode)
      printf("URL:%-50s probes: %-3d avg kB/s@sdev [\e[1;31m%.3f @%.3f\e[0m]\n",node->get_url(),node->get_num_probes(),node->get_mean(),node->get_sdev());
    else if(SM_TIME == ctx.sort_mode)
      printf("URL:%-50s probes: %-3d avg seconds@sdev [\e[1;31m%.3f @%.3f\e[0m]\n",node->get_url(),node->get_num_probes(),node->get_mean(),node->get_sdev());
    if(ctx.hdot){
      for(list<string>::iterator jt = (*it)->start_links();jt!=(*it)->end_links();jt++){
//        URL turl(*it);
        node_t* tmp = ctx.site_map[*jt];
        if(tmp)
          fprintf(ctx.hdot,"\"%s\" -> \"%s\" [label=%f]\n",(*it)->get_url(),jt->c_str(),tmp->get_mean());  
      }
    }
    if(ctx.xml_writer){
      char buf[32];
      URL parsed_url((*it)->get_url());
      xmlTextWriterStartElement(ctx.xml_writer,(xmlChar*)"url");
      xmlTextWriterWriteAttribute(ctx.xml_writer,(xmlChar*)"schema",(xmlChar*)parsed_url.get_proto());

      if(SM_SPEED == ctx.sort_mode)
        xmlTextWriterWriteAttribute(ctx.xml_writer,(xmlChar*)"measure",(xmlChar*)"speed");
      else if(SM_TIME == ctx.sort_mode)
        xmlTextWriterWriteAttribute(ctx.xml_writer,(xmlChar*)"measure",(xmlChar*)"time");
      snprintf(buf,sizeof (buf)-1,"%f",(*it)->get_mean());
      xmlTextWriterWriteAttribute(ctx.xml_writer,(xmlChar*)"mean",(xmlChar*)buf);
      snprintf(buf,sizeof (buf)-1,"%f",(*it)->get_sdev());
      xmlTextWriterWriteAttribute(ctx.xml_writer,(xmlChar*)"sdev",(xmlChar*)buf);

      xmlTextWriterWriteString(ctx.xml_writer,(xmlChar*)"\n");
      xmlTextWriterWriteCDATA(ctx.xml_writer,(xmlChar*)(*it)->get_url());
      xmlTextWriterWriteString(ctx.xml_writer,(xmlChar*)"\n");

      xmlTextWriterEndElement(ctx.xml_writer);
      xmlTextWriterWriteString(ctx.xml_writer,(xmlChar*)"\n");
    }
  }
cleanup:
  curl_easy_cleanup(ctx.hcurl);  
  curl_global_cleanup();
  if(ctx.hdot){
    fprintf(ctx.hdot,"\n}\n",ctx.url);
    fclose(ctx.hdot);
  }
  if(ctx.xml_writer){
    xmlTextWriterEndElement(ctx.xml_writer);
    xmlTextWriterEndDocument(ctx.xml_writer);
  }
  for(map<string,node_t* >::iterator it = ctx.site_map.begin();it!=ctx.site_map.end();it++){
    node_t *node = it->second;
    if(node)
      delete node;
  }
  if(ctx.url)
    free(ctx.url),ctx.url=NULL;
  if(ctx.dot_fname)
    free(ctx.dot_fname),ctx.dot_fname=NULL;
  if(ctx.xml_fname)
    free(ctx.xml_fname),ctx.xml_fname=NULL;
  if(ctx.in_fname)
    free(ctx.in_fname),ctx.xml_fname=NULL;

  if(ctx.xml_writer)
    xmlFreeTextWriter(ctx.xml_writer);
}
