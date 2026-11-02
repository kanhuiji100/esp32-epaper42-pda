#pragma once
// News reader.  Included inside ui_impl.h's anonymous namespace.
// Sources: China News Service RSS (general) + ITHome RSS (tech).  Articles are
// fetched over HTTPS (certificate not verified) and reduced to plain text.

struct NewsCat { const char* name; const char* url; uint8_t type; };  // type 0=CNS, 1=ITHome
static const NewsCat NEWS_CATS[] = {
  {"要闻", "https://www.chinanews.com.cn/rss/scroll-news.xml", 0},
  {"国内", "https://www.chinanews.com.cn/rss/china.xml", 0},
  {"国际", "https://www.chinanews.com.cn/rss/world.xml", 0},
  {"社会", "https://www.chinanews.com.cn/rss/society.xml", 0},
  {"体育", "https://www.chinanews.com.cn/rss/sports.xml", 0},
  {"财经", "https://www.chinanews.com.cn/rss/finance.xml", 0},
  {"科技", "https://www.ithome.com/rss/", 1},
};
static const int NEWS_CAT_N = (int)(sizeof(NEWS_CATS) / sizeof(NEWS_CATS[0]));
static const int NEWS_LIST_N = 10;
static const int NEWS_LINES = 11;

enum NewsView { NV_CATS, NV_LOADING, NV_LIST, NV_ARTICLE, NV_ERROR };
static NewsView nvView = NV_CATS;
static int nvCatSel = 0, nvListSel = 0, nvListTop = 0, nvPage = 0, nvPages = 1, nvN = 0;
static String nvCatName = "要闻";
static String nvMsg;
static String nvText;
static String* nvTitle = nullptr;
static String* nvLink = nullptr;
static String* nvSummary = nullptr;

static void newsAlloc() {
  if (nvTitle) return;
  nvTitle = new String[NEWS_LIST_N];
  nvLink = new String[NEWS_LIST_N];
  nvSummary = new String[NEWS_LIST_N];
}

static void newsReset() {
  nvView = NV_CATS;
  nvCatSel = nvListSel = nvListTop = 0;
  nvPage = 0; nvPages = 1; nvN = 0;
  nvMsg = "";
}

static void newsAppendUtf8(String& out, uint32_t cp) {
  if (cp < 0x80) out += (char)cp;
  else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
  else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

static String newsDecode(String s) {
  s.replace("\xE3\x80\x80", " ");   // U+3000 ideographic space -> plain space
  s.replace("&nbsp;", " "); s.replace("&#160;", " ");
  s.replace("&amp;", "&"); s.replace("&quot;", "\"");
  s.replace("&#39;", "'"); s.replace("&apos;", "'");
  s.replace("&lt;", "<"); s.replace("&gt;", ">");
  s.replace("&ldquo;", "“"); s.replace("&rdquo;", "”");
  s.replace("&hellip;", "…"); s.replace("&mdash;", "—"); s.replace("&ndash;", "–");
  int p = 0;
  while (true) {
    int i = s.indexOf("&#", p);
    if (i < 0) break;
    int e = s.indexOf(';', i);
    if (e < 0) break;
    uint32_t cp = (uint32_t)s.substring(i + 2, e).toInt();
    String rep; newsAppendUtf8(rep, cp);
    s = s.substring(0, i) + rep + s.substring(e + 1);
    p = i + rep.length();
  }
  return s;
}

static String newsStrip(String html) {
  // remove HTML comments first (ads/counters often live in them)
  {
    int c0 = 0;
    while (true) {
      int a = html.indexOf("<!--", c0);
      if (a < 0) break;
      int b = html.indexOf("-->", a + 4);
      if (b < 0) break;
      html = html.substring(0, a) + html.substring(b + 3);
      c0 = a;
    }
  }
  // remove script/style blocks
  int p = 0;
  while (true) {
    int s = html.indexOf("<script", p);
    if (s < 0) break;
    int e = html.indexOf("</script>", s);
    if (e < 0) break;
    html = html.substring(0, s) + html.substring(e + 9);
    p = s;
  }
  p = 0;
  while (true) {
    int s = html.indexOf("<style", p);
    if (s < 0) break;
    int e = html.indexOf("</style>", s);
    if (e < 0) break;
    html = html.substring(0, s) + html.substring(e + 8);
    p = s;
  }
  String out;
  out.reserve(html.length() / 2 + 16);
  int i = 0;
  while (i < (int)html.length()) {
    char c = html[i];
    if (c == '<') {
      int e = html.indexOf('>', i);
      if (e < 0) break;
      String tag = html.substring(i + 1, e);
      tag.toLowerCase();
      if (tag.startsWith("br") || tag.startsWith("/p") || tag.startsWith("/div") ||
          tag.startsWith("/h") || tag.startsWith("/li") || tag.startsWith("p ")) {
        out += '\r\n';
      }
      i = e + 1;
      continue;
    }
    out += c;
    i++;
  }
  out = newsDecode(out);
  // collapse excessive blank lines / spaces
  out.replace("\r", "");
  while (out.indexOf("\r\n\r\n\r\n") >= 0) out.replace("\r\n\r\n\r\n", "\r\n\r\n");
  return out;
}

static String newsTag(const String& item, const char* tag) {
  String open = String("<") + tag;
  int s = item.indexOf(open);
  if (s < 0) return "";
  int gt = item.indexOf('>', s);
  if (gt < 0) return "";
  int e = item.indexOf(String("</") + tag + ">", gt);
  if (e < 0) return "";
  String v = item.substring(gt + 1, e);
  v.replace("<![CDATA[", ""); v.replace("]]>", "");
  v.trim();
  return newsDecode(v);
}

static String newsHttpGet(const String& url, size_t maxLen) {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("news: no WiFi"); return ""; }
  if (ESP.getFreeHeap() < 90000) { Serial.printf("news: low heap %u\r\n", (unsigned)ESP.getFreeHeap()); return ""; }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) { Serial.println("news: begin failed"); return ""; }
  http.setTimeout(8000);
  http.setConnectTimeout(6000);
  http.addHeader("User-Agent", "Mozilla/5.0 (ESP32 ePaperPDA)");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("news: GET %s code=%d\r\n", url.c_str(), code);
    http.end();
    return "";
  }
  // Stream only maxLen bytes: some feeds (e.g. ITHome) are >300 KB.
  String body;
  body.reserve(maxLen < 8192 ? maxLen : 8192);
  WiFiClient* stream = http.getStreamPtr();
  stream->setTimeout(5);
  uint8_t buf[512];
  size_t remaining = maxLen;
  while (remaining > 0) {
    size_t want = remaining < sizeof(buf) ? remaining : sizeof(buf);
    int n = stream->readBytes(buf, want);
    if (n <= 0) break;
    body.concat((const char*)buf, n);
    remaining -= (size_t)n;
  }
  http.end();
  Serial.printf("news: GET %s len=%u code=%d\r\n", url.c_str(), (unsigned)body.length(), code);
  return body;
}

static bool newsFetchList(int cat) {
  newsAlloc();
  if (!nvTitle || !nvLink || !nvSummary) return false;
  String body = newsHttpGet(NEWS_CATS[cat].url, 40000);
  if (body.length() < 200) return false;
  nvN = 0;
  int pos = 0;
  while (nvN < NEWS_LIST_N) {
    int s = body.indexOf("<item", pos);
    if (s < 0) break;
    int e = body.indexOf("</item>", s);
    if (e < 0) break;
    String item = body.substring(s, e);
    String title = newsTag(item, "title");
    String link = newsTag(item, "link");
    String desc = newsTag(item, "description");
    if (title.length() > 0) {
      nvTitle[nvN] = title;
      nvLink[nvN] = link;
      nvSummary[nvN] = newsStrip(desc);
      if (nvSummary[nvN].length() > 400) nvSummary[nvN].remove(400);
      nvN++;
    }
    pos = e + 7;
  }
  Serial.printf("news: parsed %d items from %s\r\n", nvN, NEWS_CATS[cat].name);
  return nvN > 0;
}

static String newsExtractArticle(const String& html, uint8_t type) {
  String cut = html;
  if (type == 0) {
    int i = html.indexOf("class=\"left_zw\"");
    if (i < 0) i = html.indexOf("cont_1_1_2");
    if (i < 0) i = html.indexOf("class=\"content");
    if (i >= 0) {
      int gt = html.indexOf('>', i);
      if (gt > 0) cut = html.substring(gt + 1);   // skip past the opening tag
    }
  } else {
    int i = html.indexOf("id=\"paragraph\"");
    if (i < 0) i = html.indexOf("class=\"post_content\"");
    if (i < 0) i = html.indexOf("<article");
    if (i >= 0) cut = html.substring(i);
  }
  String out;
  int p = 0;
  while (out.length() < 6000) {
    int s = cut.indexOf("<p", p);
    if (s < 0) break;
    int e = cut.indexOf("</p>", s);
    if (e < 0) break;
    String para = newsStrip(cut.substring(s, e));
    para.trim();
    if (para.length() > 0) { out += para; out += "\r\n\r\n"; }
    p = e + 4;
  }
  if (out.length() < 60) out = newsStrip(cut);
  out.trim();
  if (out.length() > 6000) out.remove(6000);
  return out;
}

static int newsCountPages(const String& s) {
  int start = 0, pages = 1;
  while (start < (int)s.length() && pages < 200) {
    int next = start;
    layoutText(20, 0, 360, NOTE_LINEH, NEWS_LINES, s, 1, INK, false, start, &next);
    if (next <= start) break;
    start = next;
    if (start < (int)s.length()) pages++;
  }
  return pages;
}

static String newsTruncate(const String& s, int maxBytes) {
  if ((int)s.length() <= maxBytes) return s;
  int n = maxBytes;
  while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) n--;
  return s.substring(0, n) + "…";
}


// ---------------- offline cache on /ext/cache/news ----------------
// Lists are one file per category (L<cat>.txt, 3 lines per item); articles are
// keyed by a hash of the link. Every write is compare-before-write, so opening
// the news app repeatedly does NOT keep erasing the same flash blocks.
static const char* NEWS_CACHE_DIR = "/ext/cache/news";
static bool nvCached = false;

static uint32_t newsHash(const String& s) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < s.length(); i++) { h ^= (uint8_t)s[i]; h *= 16777619u; }
  return h;
}

static String newsReadFile(const String& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return "";
  String out;
  char buf[512];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.concat(buf, n);
  fclose(f);
  return out;
}

static bool newsWriteIfChanged(const String& path, const String& data) {
  if (!storage::extReady() || data.length() == 0) return false;
  mkdir("/ext/cache", 0777);
  mkdir(NEWS_CACHE_DIR, 0777);
  String old = newsReadFile(path);
  if (old.length() == data.length() && old == data) return true;   // nothing changed
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  size_t w = fwrite(data.c_str(), 1, data.length(), f);
  fclose(f);
  return w == data.length();
}

static String newsListPath(int cat) {
  return String(NEWS_CACHE_DIR) + "/L" + String(cat) + ".txt";
}

static void newsCacheSaveList(int cat) {
  String d;
  d.reserve(nvN * 96);
  for (int i = 0; i < nvN; i++) {
    String t = nvTitle[i]; t.replace("\n", " "); t.replace("\r", " ");
    String m = nvSummary[i]; m.replace("\n", " "); m.replace("\r", " ");
    d += t; d += '\n'; d += nvLink[i]; d += '\n'; d += m; d += '\n';
  }
  newsWriteIfChanged(newsListPath(cat), d);
}

static String newsClean(const String& in) {
  String t = in;
  t.replace("\r", "");               // stray CRs render as tofu on the panel
  t.replace("\xE3\x80\x80", " ");   // U+3000 -> space
  t.replace("\xC2\xA0", " ");       // NBSP
  t.replace("\xEF\xBF\xBD", "");    // U+FFFD (decode error) -> drop
  while (t.indexOf("\n\n\n") >= 0) t.replace("\n\n\n", "\n\n");
  return t;
}

static bool newsCacheLoadList(int cat) {
  newsAlloc();                       // list buffers live on the heap and may not exist yet
  if (!nvTitle || !nvLink || !nvSummary) return false;
  String d = newsReadFile(newsListPath(cat));
  if (d.length() < 8) return false;
  nvN = 0;
  int p = 0;
  while (nvN < NEWS_LIST_N) {
    int a = d.indexOf('\n', p); if (a < 0) break;
    int b = d.indexOf('\n', a + 1); if (b < 0) break;
    int c = d.indexOf('\n', b + 1); if (c < 0) break;
    String t = d.substring(p, a);
    String l = d.substring(a + 1, b);
    String m = d.substring(b + 1, c);
    if (t.length()) { nvTitle[nvN] = newsClean(t); nvLink[nvN] = l; nvSummary[nvN] = newsClean(m); nvN++; }
    p = c + 1;
  }
  return nvN > 0;
}

static String newsArticlePath(const String& link) {
  char b[16];
  snprintf(b, sizeof(b), "%08x", (unsigned)newsHash(link));
  return String(NEWS_CACHE_DIR) + "/" + b + ".txt";
}

// Keep the article cache bounded so it never fills the chip.
static void newsCacheEvict() {
  DIR* d = opendir(NEWS_CACHE_DIR);
  if (!d) return;
  int n = 0;
  String first;
  struct dirent* e;
  while ((e = readdir(d)) != nullptr) {
    if (e->d_name[0] == 'L' || e->d_name[0] == '.') continue;
    if (!strstr(e->d_name, ".txt")) continue;
    n++;
    if (first.length() == 0) first = e->d_name;
  }
  closedir(d);
  if (n >= 60 && first.length())
    remove((String(NEWS_CACHE_DIR) + "/" + first).c_str());
}

static void newsCacheSaveArticle(const String& link, const String& text) {
  if (text.length() < 200) return;   // don't cache a failed/thin extraction
  newsCacheEvict();
  newsWriteIfChanged(newsArticlePath(link), text);
}

static void newsCacheClear() {
  DIR* d = opendir(NEWS_CACHE_DIR);
  if (!d) return;
  struct dirent* e;
  while ((e = readdir(d)) != nullptr) {
    if (e->d_name[0] == '.') continue;
    remove((String(NEWS_CACHE_DIR) + "/" + e->d_name).c_str());
  }
  closedir(d);
}

static void newsLoadList(int cat) {
  nvCatName = NEWS_CATS[cat].name;
  nvCached = false;
  nvListSel = 0;
  nvListTop = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (newsCacheLoadList(cat)) { nvCached = true; nvView = NV_LIST; return; }
    nvMsg = "WiFi 未连接，且没有缓存"; nvView = NV_ERROR; return;
  }
  nvView = NV_LOADING; nvMsg = String("正在获取 ") + NEWS_CATS[cat].name + " …";
  renderAndCommit(true);
  if (!newsFetchList(cat)) {
    if (newsCacheLoadList(cat)) { nvCached = true; nvView = NV_LIST; return; }
    nvMsg = "获取失败，且没有缓存"; nvView = NV_ERROR; return;
  }
  newsCacheSaveList(cat);
  nvView = NV_LIST;
}

static void newsLoadArticle(int idx) {
  if (idx < 0 || idx >= nvN) return;
  nvView = NV_LOADING; nvMsg = "正在打开文章 …";
  renderAndCommit(true);
  String text;
  nvCached = false;
  if (WiFi.status() == WL_CONNECTED && nvLink[idx].startsWith("http")) {
    String html = newsHttpGet(nvLink[idx], 60000);
    if (html.length() > 200) text = newsExtractArticle(html, NEWS_CATS[nvCatSel].type);
  }
  if (text.length() >= 60) {
    newsCacheSaveArticle(nvLink[idx], text);
  } else {
    String c = newsReadFile(newsArticlePath(nvLink[idx]));
    if (c.length() >= 60) { text = c; nvCached = true; }
  }
  if (text.length() < 60) text = nvSummary[idx];
  if (text.length() < 20) text = "（无正文，且没有缓存）";
  text = newsClean(text);
  nvText = nvTitle[idx] + "\r\n\r\n" + text;
  nvPages = newsCountPages(nvText);
  nvPage = 0;
  nvView = NV_ARTICLE;
}

static void drawNewsApp() {
  fillRect(0, 0, W, H, PAPER);
  if (nvView == NV_CATS) {
    drawStatus("新闻", (String(NEWS_CAT_N) + "类").c_str());
    drawText(20, 28, "新闻分类", 2, INK);
    drawText(20, 62, "中国新闻网 · IT之家", 1, INK);
    const int top = 84, ih = 24;
    for (int i = 0; i < NEWS_CAT_N; i++) {
      int y = top + i * ih;
      bool sel = (i == nvCatSel);
      if (sel) fillRect(12, y, 376, 22, INK);
      uint8_t ink = sel ? PAPER : INK;
      drawText(30, y + 3, NEWS_CATS[i].name, 1, ink);
      if (sel) drawText(366, y + 3, ">", 1, ink);
    }
    hline(12, 270, 376, INK);
    drawText(14, 276, "↑↓ 选择   OK 获取   Back 返回", 1, INK);
    return;
  }
  if (nvView == NV_LOADING) {
    drawStatus("新闻", nvCatName.c_str());
    drawText(20, 120, nvMsg, 1, INK);
    hline(12, 270, 376, INK);
    drawText(14, 276, "请稍候…", 1, INK);
    return;
  }
  if (nvView == NV_ERROR) {
    drawStatus("新闻", "错误");
    drawText(20, 110, nvMsg, 1, INK);
    drawText(20, 140, "OK 重试   Back 返回", 1, INK);
    hline(12, 270, 376, INK);
    drawText(14, 276, "Back 返回分类", 1, INK);
    return;
  }
  if (nvView == NV_LIST) {
    drawStatus("新闻", (nvCatName + (nvCached ? "·缓存" : "")).c_str());
    const int top = 34, ih = 38, rows = 6;
    for (int row = 0; row < rows; row++) {
      int i = nvListTop + row;
      if (i >= nvN) break;
      int y = top + row * ih;
      bool sel = (i == nvListSel);
      if (sel) fillRect(8, y, 384, 36, INK);
      uint8_t ink = sel ? PAPER : INK;
      drawText(14, y + 3, String(i + 1) + ".", 1, ink);
      layoutText(34, y + 2, 356, 18, 2, nvTitle[i], 1, ink, true, 0, nullptr);
    }
    hline(12, 270, 376, INK);
    drawText(14, 276, "↑↓ 选择   OK 阅读   Back 分类", 1, INK);
    drawText(330, 276, String(nvListSel + 1) + "/" + String(nvN), 1, INK);
    return;
  }
  // article
  drawStatus("新闻", (String(nvCached ? "缓存 " : "") + String(nvPage + 1) + "/" + String(nvPages)).c_str());
  int start = 0;
  for (int p = 0; p < nvPage; p++) {
    int next = start;
    layoutText(20, 0, 360, NOTE_LINEH, NEWS_LINES, nvText, 1, INK, false, start, &next);
    if (next <= start) break;
    start = next;
  }
  layoutText(20, 32, 360, NOTE_LINEH, NEWS_LINES, nvText, 1, INK, true, start, nullptr);
  hline(12, 270, 376, INK);
  drawText(14, 276, "←→ 翻页   Back 返回列表", 1, INK);
}

static void newsKey(const String& k) {
  if (nvView == NV_CATS) {
    if (k == "up") nvCatSel = (nvCatSel + NEWS_CAT_N - 1) % NEWS_CAT_N;
    else if (k == "down") nvCatSel = (nvCatSel + 1) % NEWS_CAT_N;
    else if (k == "ok") newsLoadList(nvCatSel);
    else if (k == "back" || k == "home") app = App::HOME;
    return;
  }
  if (nvView == NV_LOADING) return;
  if (nvView == NV_ERROR) {
    if (k == "ok") newsLoadList(nvCatSel);
    else if (k == "back" || k == "home") nvView = NV_CATS;
    return;
  }
  if (nvView == NV_LIST) {
    if (k == "up") {
      if (nvListSel > 0) { nvListSel--; if (nvListSel < nvListTop) nvListTop = nvListSel; }
    } else if (k == "down") {
      if (nvListSel + 1 < nvN) { nvListSel++; if (nvListSel >= nvListTop + 6) nvListTop = nvListSel - 5; }
    } else if (k == "ok") newsLoadArticle(nvListSel);
    else if (k == "back" || k == "home") nvView = NV_CATS;
    return;
  }
  // article
  if (k == "left" || k == "up") { if (nvPage > 0) nvPage--; }
  else if (k == "right" || k == "down") { if (nvPage + 1 < nvPages) nvPage++; }
  else if (k == "back") nvView = NV_LIST;
  else if (k == "home") app = App::HOME;
}
