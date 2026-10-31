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
    int i = html.indexOf("cont_1_1_2");
    if (i < 0) i = html.indexOf("class=\"content");
    if (i >= 0) cut = html.substring(i);
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

static void newsLoadList(int cat) {
  nvCatName = NEWS_CATS[cat].name;
  if (WiFi.status() != WL_CONNECTED) { nvMsg = "WiFi 未连接"; nvView = NV_ERROR; return; }
  nvView = NV_LOADING; nvMsg = String("正在获取 ") + NEWS_CATS[cat].name + " …";
  renderAndCommit(true);
  if (!newsFetchList(cat)) { nvMsg = "获取失败，请重试"; nvView = NV_ERROR; return; }
  nvListSel = 0;
  nvListTop = 0;
  nvView = NV_LIST;
}

static void newsLoadArticle(int idx) {
  if (idx < 0 || idx >= nvN) return;
  nvView = NV_LOADING; nvMsg = "正在打开文章 …";
  renderAndCommit(true);
  String text;
  if (nvLink[idx].startsWith("http")) {
    String html = newsHttpGet(nvLink[idx], 60000);
    if (html.length() > 200) text = newsExtractArticle(html, NEWS_CATS[nvCatSel].type);
  }
  if (text.length() < 60) text = nvSummary[idx];
  if (text.length() < 20) text = "（无法获取正文，请返回重试）";
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
    drawStatus("新闻", nvCatName.c_str());
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
  drawStatus("新闻", (String(nvPage + 1) + "/" + String(nvPages)).c_str());
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
