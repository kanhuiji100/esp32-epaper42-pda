# News reader

The **新闻** app fetches today's Chinese news over Wi-Fi, lets you pick a
category, then a headline, and reads the article as plain text on the e-paper.

## Categories and sources

| Category | Source |
|---|---|
| 要闻 / 国内 / 国际 / 社会 / 体育 / 财经 | China News Service RSS (`chinanews.com.cn/rss/...`) |
| 科技 | ITHome RSS (`ithome.com/rss/`) |

The feeds are public RSS; no API key is required. Each list shows the first
6–10 items the feed returns, with the headline and (where available) a summary.

![News categories](images/news_cats.png)

The category screen shows the source subtitle and the number of categories in the
status bar. Each list page shows 6 headlines with up to two lines per title, so
long Chinese headlines use the full width; the list scrolls with `↑↓` and shows
the position (e.g. `8/10`) at the bottom right.

![News list](images/news_list.png)

## Reading an article

Selecting a headline fetches the article page over HTTPS and converts it to
plain text (paragraphs, no images). Long articles are paginated at 11 lines per
page. If the page cannot be parsed, the RSS summary is shown instead.

![News article](images/news_article.png)

## Controls

| Key | Categories | List | Article |
|---|---|---|---|
| `←↑↓→` | `↑↓` choose | `↑↓` choose | `←→` page |
| `OK` | fetch category | open article | — |
| `Back` | Home | back to categories | back to list |

Web remote: the **新闻** shortcut plus the D-pad.
Serial: `app news`, then `key ...` as above.

## Offline cache

Lists and articles are cached on the external flash under `/ext/cache/news`:

- `L<cat>.txt` — the last list for each category (3 lines per item:
  title / link / summary).
- `<hash>.txt` — article text, keyed by an FNV-1a hash of the link.

When a fetch fails (no Wi-Fi or upstream error) the app shows the cached copy and
marks the status bar with **缓存**. Cached lists can also be opened directly with
`news offline <cat>` from the serial console.

![Cached article](images/news_cached.png)

### Flash wear

- Every write is **compare-before-write**: if the fetched list/article is
  identical to what is already stored, nothing is written. Re-opening the news
  app therefore does not keep erasing the same blocks.
- Lists are one small file per category; articles are capped at ~60 files and the
  oldest is evicted, so the cache cannot fill the chip.
- Everything goes through LittleFS (log-structured, wear-levelled).

## Notes and limits

- Requires an internet connection; if Wi-Fi is down the app shows an error.
- HTTPS uses `setInsecure()` (no certificate validation) to keep the firmware
  small; the content is public news, not credentials.
- No offline cache yet: lists and articles are kept in RAM only.
- Article extraction is generic (finds the main paragraph container); unusual
  page layouts may fall back to the RSS summary.
- Images, video and interactive content are ignored.
- The feed order/content is controlled by the source; this app does not filter
  or editorialize.
