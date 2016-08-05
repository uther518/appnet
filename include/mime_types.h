

#ifndef _MIME_TYPES_H_
#define _MIME_TYPES_H_

#define MIME_TYPES_NUM 79

const char *mime_types[MIME_TYPES_NUM][2] = {
    {"text/html", "html htm shtml"},
    {"text/css", "css"},
    {"text/xml", "xml"},
    {"image/gif", "gif"},
    {"image/jpeg", "jpeg jpg"},
    {"application/javascript", "js"},
    {"application/atom+xml", "atom"},
    {"application/rss+xml", "rss"},
    {"text/mathml", "mml"},
    {"text/plain", "txt"},
    {"text/vnd.sun.j2me.app-descriptor", "jad"},
    {"text/vnd.wap.wml", "wml"},
    {"text/x-component", "htc"},
    {"image/png", "png"},
    {"image/tiff", "tif tiff"},
    {"image/vnd.wap.wbmp", "wbmp"},
    {"image/x-icon", "ico"},
    {"image/x-jng", "jng"},
    {"image/x-ms-bmp", "bmp"},
    {"image/svg+xml", "svg svgz"},
    {"image/webp", "webp"},
    {"application/font-woff", "woff"},
    {"application/java-archive", "jar war ear"},
    {"application/json", "json"},
    {"application/mac-binhex40", "hqx"},
    {"application/msword", "doc"},
    {"application/pdf", "pdf"},
    {"application/postscript", "ps eps ai"},
    {"application/rtf", "rtf"},
    {"application/vnd.apple.mpegurl", "m3u8"},
    {"application/vnd.ms-excel", "xls"},
    {"application/vnd.ms-fontobject", "eot"},
    {"application/vnd.ms-powerpoint", "ppt"},
    {"application/vnd.wap.wmlc", "wmlc"},
    {"application/vnd.google-earth.kml+xml", "kml"},
    {"application/vnd.google-earth.kmz", "kmz"},
    {"application/x-7z-compressed", "7z"},
    {"application/x-cocoa", "cco"},
    {"application/x-java-archive-diff", "jardiff"},
    {"application/x-java-jnlp-file", "jnlp"},
    {"application/x-makeself", "run"},
    {"application/x-perl", "pl pm"},
    {"application/x-pilot", "prc pdb"},
    {"application/x-rar-compressed", "rar"},
    {"application/x-redhat-package-manager", "rpm"},
    {"application/x-sea", "sea"},
    {"application/x-shockwave-flash", "swf"},
    {"application/x-stuffit", "sit"},
    {"application/x-tcl", "tcl tk"},
    {"application/x-x509-ca-cert", "der pem crt"},
    {"application/x-xpinstall", "xpi"},
    {"application/xhtml+xml", "xhtml"},
    {"application/xspf+xml", "xspf"},
    {"application/zip", "zip"},
    {"application/octet-stream", "bin exe dll"},
    {"application/octet-stream", "deb"},
    {"application/octet-stream", "dmg"},
    {"application/octet-stream", "iso img"},
    {"application/octet-stream", "msi msp msm"},
    {"audio/midi", "mid midi kar"},
    {"audio/mpeg", "mp3"},
    {"audio/ogg", "ogg"},
    {"audio/x-m4a", "m4a"},
    {"audio/x-realaudio", "ra"},
    {"video/3gpp", "3gpp 3gp"},
    {"video/mp2t", "ts"},
    {"video/mp4", "mp4"},
    {"video/mpeg", "mpeg mpg"},
    {"video/quicktime", "mov"},
    {"video/webm", "webm"},
    {"video/x-flv", "flv"},
    {"video/x-m4v", "m4v"},
    {"video/x-mng", "mng"},
    {"video/x-ms-asf", "asx asf"},
    {"video/x-ms-wmv", "wmv"},
    {"video/x-msvideo", "avi"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document",
     "docx"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
     "xlsx"},
    {"application/"
     "vnd.openxmlformats-officedocument.presentationml.presentation",
     "pptx"}};

static inline int getExtMimeType(char *ext, char *ctype) {
  if (!ext)
    return 0;

  int i;
  char *ret;
  for (i = 0; i < MIME_TYPES_NUM; i++) {
    if (!mime_types[i]) {
      return 0;
    }

    ret = strstr(mime_types[i][1], ext);
    if (ret) {
      memcpy(ctype, mime_types[i][0], strlen(mime_types[i][0]));
      return 1;
    }
  }
  return 0;
}

static inline int getMimeType(char *uri, char *ctype) {
  char *pos;
  pos = strstr(uri, "?");

  char file_name[1024] = {0};
  char ext[16] = {0};
  char *ptr = strrchr(uri, '/');
  if (ptr == NULL)
    return 0;

  int len = uri - ptr + strlen(uri);
  if (pos != NULL) {
    int pos_len = pos - ptr;
    len = pos_len - 1;
  }

  memcpy(file_name, ptr + 1, len);
  char *ext_ptr = strrchr(file_name, '.');
  if (ext_ptr == NULL)
    return 0;
  sprintf(ext, "%s", ext_ptr + 1);

  int ret = getExtMimeType(ext, ctype);
  return ret;
}

#endif /* _MIME_TYPES_H_ */
