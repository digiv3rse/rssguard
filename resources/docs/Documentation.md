# 🔥 RSS Guard Documentation 🔥

Welcome to RSS Guard documentation. You can find everything about the application right here.

There is a [Discord server](https://discord.gg/7xbVMPPNqH) for user communication.

## Table of Contents
- [What is RSS Guard?](#wirss)
- [Downloads](#dwn)
- [Supported Operating Systems](#sos)
- [Major Features](#mfe)
    - [Supported Feed Readers](#sfr)
    - [Article Filtering](#fltr)
    - [Websites Scraping](#scrap)
    - [Notifications](#notif)
    - [Database Backends](#datab)
    - [User Data Portability](#userd)
    - [Built-in Web Browser with AdBlock](#webb)
- [Minor Features](#mife)
    - [Files Downloader](#downl)
    - [Node.js](#node)
    - [Labels](#lbls)
    - [Skins](#skin)
    - [GUI Tweaking](#guit)
    - [Command Line Interface (CLI)](#cli)
- [For Contributors and Other Topics](#contrib)
    - [Donations](#donat)
    - [Compiling RSS Guard](#compil)
    - [Plugin API](#papi)
    - [Reporting Bugs or Feature Requests](#reprt)
    - [Localization](#locali)
    - [Migrating data](#migratt)

<hr style="margin: 40px;"/>

## What is RSS Guard? <a id="wirss"></a>
RSS Guard is an [open-source](https://en.wikipedia.org/wiki/Open_source) [cross-platform](#sos) [multi-protocol](#sfr) desktop feed reader. It is able to fetch feeds in RSS/RDF/ATOM/JSON formats. RSS Guard is developed on top of the [Qt library](http://qt-project.org).

## Downloads <a id="dwn"></a>
Official place to download RSS Guard is at [Github Releases](https://github.com/martinrotter/rssguard/releases). You can also download the [development (beta) build](https://github.com/martinrotter/rssguard/releases/tag/devbuild), which is updated automatically every time the source code is updated.

RSS Guard is also available for [many Linux distributions](https://repology.org/project/rssguard/versions), and even via [Flathub](https://flathub.org/apps/details/com.github.rssguard).

I highly recommend to download RSS Guard only from reputable sources.

## Supported Operating Systems <a id="sos"></a>
RSS Guard is a cross-platform application, and at this point it is known to work on:
* Windows 10+
* GNU/Linux with glibc 2.31+ (including PinePhone and other Linux-based phone operating systems)
* BSD (FreeBSD, OpenBSD, NetBSD, etc.)
* macOS 10.14+
* OS/2 (ArcaOS, eComStation)

## Major Features <a id="mfe"></a>

### Supported Feed Readers <a id="sfr"></a>
RSS Guard is multi-account application and supports many web-based feed readers via [built-in plugins](#papi). One of the plugins, of course, provides the support for standard **RSS/ATOM/JSON** feeds with the set of features everyone would expect from classic feed reader, like OPML support, etc.

I organized the supported web-based feed readers into an elegant table:

| Service | Two-way Synchronization | [Intelligent Synchronization Algorithm](#intel) (ISA) <sup>1</sup> | Synchronized Labels <sup>2</sup> <a id="sfrl"></a> | OAuth <sup>4</sup> |
| :---              | :---:  | :---: | :---: | :---:
| Feedly            | ✅ | ✅ | ✅ | ✅ (only for official binaries)
| Gmail             | ✅ | ✅ | ❌ | ✅
| Google Reader API <sup>3</sup> | ✅ | ✅ | ✅ | ✅ (only for Inoreader)
| Nextcloud News    | ✅ | ❌ | ❌ | ❌
| Tiny Tiny RSS     | ✅ | ✅ | ✅ | ❌

<sup>1</sup> Some plugins support next-gen intelligent synchronization algorithm (ISA) which has some benefits, as it usually offers superior synchronization speed, and transfers much less data over your network connection. <a id="intel"></a>

<img alt="alt-img" src="images/intel.png" width="350px">

With ISA, RSS Guard only downloads articles which are new or were updated. While the old algorithm usually always fetch all available articles, even if they are not needed, which leads to unnecessary overload of your network connection and RSS Guard.

<sup>2</sup> Note that [labels](#lbls) are supported for all plugins, but for some plugins they are local-only, and are not synchronized with the service. Usually because service itself does not support the feature.

<sup>3</sup> Tested services are:
* Bazqux
* Reedah
* Inoreader
* Miniflux
* TheOldReader
* FreshRSS

<sup>4</sup> [OAuth](https://en.wikipedia.org/wiki/OAuth) is secure way of authenticating users in online applications.

### Article Filtering <a id="fltr"></a>
Sometimes you need to automatically tweak incoming article - mark it starred, remove ads from its contents or simply ignore it. That's where filtering feature comes in.

<img alt="alt-img" src="images/filters-dialog.png" width="600px">

#### Writing article filter
Article filters are small scripts which are executed automatically when articles/feeds are downloaded. Article filters are `JavaScript` pieces of code which must provide function with prototype:

```js
function filterMessage() { }
```

The function should be fast and must return values which belong to enumeration [`FilteringAction`](#FilteringAction-enum).

Each article is accessible in your script via global variable named `msg` of type `MessageObject`, see [this file](https://github.com/martinrotter/rssguard/blob/master/src/librssguard/core/messageobject.h) for the declaration. Some properties are writeable, allowing you to change contents of the article before it is written to RSS Guard's DB. You can mark article important, change its description, perhaps change author name or even assign some label to it!!!

Almost any changes you make are synchronized back to feed service if the particular RSS Guard plugin supports it.

You can use [special placeholders](#userd-plac) within article filter.

Also, there is a special variable named `utils`. This variable is of type `FilterUtils` and offers some useful [utility functions](#utils-object) for you to use in your filters.

RSS Guard allows to use the list of labels assigned to each article. You can, therefore, execute actions in your filtering script, based on which labels are assigned to the article. The property is called `assignedLabels` and is array of [`Label`](#Label-class) objects.

Passed article also offers special function:

```js
Boolean MessageObject.isAlreadyInDatabase(DuplicateCheck)
```

which allows you to perform runtime check for existence of the article in RSS Guard's database. The parameter is value from enumeration [`DuplicateCheck`](#duplicatecheck-enum) and specifies how exactly you want to match your article.

For example, if you want to check if there is already another article with same author in database, you should call `msg.isAlreadyInDatabase(MessageObject.SameAuthor)`. Values of the enumeration can be combined via bitwise OR (`|`) operator in single call, like this:

```js
msg.isAlreadyInDatabase(MessageObject.SameAuthor | MessageObject.SameUrl)
```

Here is the reference of methods and properties of types available in your filtering scripts.

#### `MessageObject` class
| Type      | Name(Parameter)               | Return value  | Read-only | Synchronized | Description
| :---      | :---                          | :---          | :---:     | :---:        | ---
| Property  | `assignedLabels`              | `Array<Label>`| ✅         | ✅            | List of labels assigned to the message/article.
| Property  | `availableLabels`             | `Array<Label>`| ✅         | ❌            | List of labels which are currently available and can be assigned to the message. Available in RSS Guard 3.8.1+.
| Property  | `feedCustomId`                | `String`      | ✅         | ❌            | Service-specific ID of the feed which this message belongs to.
| Property  | `accountId`                   | `Number`      | ✅         | ❌            | RSS Guard's ID of the account activated in the program. This property is highly advanced and you probably do not need to use it at all.
| Property  | `id`                          | `Number`      | ✅         | ❌            | ID assigned to the message in RSS Guard local database.
| Property  | `customId`                    | `String`      | ❌         | ❌            | ID of the message as provided by the remote service or feed file.
| Property  | `title`                       | `String`      | ❌         | ❌            | Title of the message.
| Property  | `url`                         | `String`      | ❌         | ❌            | URL of the message.
| Property  | `author`                      | `String`      | ❌         | ❌            | Author of the message.
| Property  | `contents`                    | `String`      | ❌         | ❌            | Contents of the message.
| Property  | `rawContents`                 | `String`      | ❌         | ❌            | This is RAW contents of the message as it was obtained from remote service/feed. You can expect raw `XML` or `JSON` element data here. Note that this attribute has some value only if `runningFilterWhenFetching` returns `true`. In other words, this attribute is not persistently stored inside RSS Guard's DB. Also, this attribute is artificially filled with ATOM-like data when testing the filter.
| Property  | `score`                       | `Number`      | ❌         | ❌            | Arbitrary number in range \<0.0, 100.0\>. You can use this number to sort messages in a custom fashion as this attribute also has its own column in articles list.
| Property  | `created`                     | `Date`        | ❌         | ❌            | Date/time of the message.
| Property  | `isRead`                      | `Boolean`     | ❌         | ✅            | Is message read?
| Property  | `isImportant`                 | `Boolean`     | ❌         | ✅            | Is message important?
| Property  | `isDeleted`                   | `Boolean`     | ❌         | ❌            | Is message placed in recycle bin?
| Method    | `isAlreadyInDatabase(DuplicateCheck)` | `Boolean` | ❌         | ❌            | Allows you to test if this particular message is already stored in RSS Guard's DB.
| Method    | `findLabelId(String)`         | `String`     | ❌         | ❌            | You enter title of the label and method returns `customId` of label which then can be used in  `assignLabel()` and `deassignLabel` methods.
| Method    | `assignLabel(String)`         | `Boolean`     | ❌         | ❌            | Assigns label to this message. The passed `String` value is the `customId` property of `Label` type. See its API reference for relevant info.
| Method    | `deassignLabel(String)`       | `Boolean`     | ❌         | ❌            | Removes label from this message. The passed `String` value is the `customId` property of `Label` type. See its API reference for relevant info.
| Property  | `runningFilterWhenFetching`   | `Boolean`     | ✅         | ❌            | Returns `true` if current run of the message filter is done when message is fetched. Returns `false` if message filter runs manually, for example from `Article filters` window.

#### `Label` class
| Type      | Name          | Return value  | Read-only | Description
| :---      | :---          | :---          | :---:     | ---
| Property  | `title`       | `String`      | ✅        | Label title.
| Property  | `customId`    | `String`      | ✅        | Service-specific ID of this label. This ID is used as unique identifier for the label and is particularly useful if you want to assign/unassign the message label.
| Property  | `color`       | `Color`       | ✅        | Label color. Note that type `color` has its documentation [here](https://doc.qt.io/qt-5/qml-color.html).

#### `FilteringAction` enum
| Enumerant name    | Integer value | Description
| :---              | :---          | ---
| `Accept`          | 1             | Message is accepted and will be added to DB or updated in DB.
| `Ignore`          | 2             | Message is ignored and will **NOT** be added or updated in DB, but will also not be purged if already exists.
| `Purge`           | 4             | Existing message is purged from the DB completely. Behaves like `Ignore` when there is new incoming message.

Note that `MessageObject` attributes are synchronized with service even if you return `Purge` or `Ignore`. In other words, even if your filter ignores the article, you can still tweak its properties which will be synchronized back to your server.

#### `DuplicateCheck` enum
| Enumerant name    | Integer value | Description
| :---              | :---          | ---
| `SameTitle`       | 1             | Check if message has same title as some another messages.
| `SameUrl`         | 2             | Check if message has same URL as some another messages.
| `SameAuthor`      | 4             | Check if message has same author as some another messages.
| `SameDateCreated` | 8             | Check if message has same date of creation as some another messages.
| `AllFeedsSameAccount` | 16        | Perform the check across all feeds from your account, not just "current" feed.
| `SameCustomId`    | 32            | Check if message with same custom ID exists in RSS Guard's DB.

#### `utils` object
| Type      | Name(Parameter)           | Return value  | How to call                               | Description
| :---      | :---                      | :---          | :---                                      | ---
| Method    | `hostname()`              | `String`      | `utils.hostname()`                        | Returns name of your PC.
| Method    | `fromXmlToJson(String)`   | `String`      | `utils.fromXmlToJson('<h1>hello</h1>')`   | Converts `XML` string into `JSON`.
| Method    | `parseDateTime(String)`   | `Date`        | `utils.parseDateTime('2020-02-24T08:00:00')`  | Converts textual date/time representation into proper `Date` object.
| Method    | `runExecutableGetOutput(String, String[])`   | `String`        | `utils.runExecutableGetOutput('cmd.exe', ['/c', 'dir'])`  | Launches external executable with optional parameters, reads its standard output and returns the output when executable finishes.

#### Examples
Accept only messages/articles with title containing "Series Name" or "Another series" in it (whitelist):
```js
var whitelist = [
  'Series Name', 'Another series'
];
function filterMessage() {
  if (whitelist.some(i => msg.title.indexOf(i) != -1)) {
    return MessageObject.Accept;
  } else {
    return MessageObject.Ignore;
  }
}
```

Accept only messages/articles with title NOT containing "Other Series Name" or "Some other title" in it (blacklist):
```js
var blacklist = [
  'Other Series Name', 'Some other title'
];
function filterMessage() {
  if (blacklist.some(i => msg.title.indexOf(i) != -1)) {
    return MessageObject.Ignore;
  } else {
    return MessageObject.Accept;
  }
}
```

Accept only messages/articles from "Bob", while also mark them "important":
```js
function filterMessage() {
  if (msg.author == "Bob") {
    msg.isImportant = true;
    return MessageObject.Accept;
  }
  else {
    return MessageObject.Ignore;
  }
}
```

Replace all "dogs" with "cats"!
```js
function filterMessage() {
  msg.title = msg.title.replace("dogs", "cats");
  return MessageObject.Accept;
}
```

Use published element instead of updated element (for ATOM entries only):
```js
function filterMessage() {
  // Read raw contents of message and
  // convert to JSON.
  json = utils.fromXmlToJson(msg.rawContents);
  jsonObj = JSON.parse(json)

  // Read published date and parse it.
  publishedDate = jsonObj.entry.published.__text;
  parsedDate = utils.parseDateTime(publishedDate);

  // Set new date/time for message and
  // proceed.
  msg.created = parsedDate;
  return MessageObject.Accept;
}
```

Dump RAW data of each message to RSS Guard's [debug output](#reprt):
```js
function filterMessage() {
  console.log(msg.rawContents);
  return MessageObject.Accept;
}
```

When running the above script for Tiny Tiny RSS, it produces the following debug output:
```
...
time="    34.360" type="debug" -> feed-downloader: Hooking message took 4 microseconds.
time="    34.361" type="debug" -> {"always_display_attachments":false,"attachments":[],"author":"Aleš Kapica","comments_count":0,"comments_link":"","content":"<p>\nNaposledy jsem psal o čuňačení v MediaWiki asi před půl rokem, kdy jsem chtěl upozornit na to, že jsem přepracoval svoji původní šablonu Images tak, aby bylo možné používat výřezy z obrázků a stránek generovaných z DjVu a PDF dokumentů. Blogpost nebyl nijak extra hodnocen, takže mě vcelku nepřekvapuje, jak se do hlavní vývojové větve MediaWiki dostávají čím dál větší prasečiny.\n</p>","feed_id":"5903","feed_title":"abclinuxu - blogy","flavor_image":"","flavor_stream":"","guid":"{\"ver\":2,\"uid\":\"52\",\"hash\":\"SHA1:5b49e4d8f612984889ba25e7834e80604c795ff8\"}","id":6958843,"is_updated":false,"labels":[],"lang":"","link":"http://www.abclinuxu.cz/blog/kenyho_stesky/2021/1/cunacime-v-mediawiki-responzivni-obsah-ii","marked":false,"note":null,"published":false,"score":0,"tags":[""],"title":"Čuňačíme v MediaWiki - responzivní obsah II.","unread":true,"updated":1610044674}
time="    34.361" type="debug" -> feed-downloader: Running filter script, it took 348 microseconds.
time="    34.361" type="debug" -> feed-downloader: Hooking message took 4 microseconds.
time="    34.361" type="debug" -> {"always_display_attachments":false,"attachments":[],"author":"kol-ouch","comments_count":0,"comments_link":"","content":"Ahoj, 1. 6. se blíží, tak začínám řešit co s bambilionem fotek na google photos. \n<p class=\"separator\"></p>\nZa sebe můžu říct, že gp mi vyhovují - ne snad úplně tím, že jsou zadarmo, ale hlavně způsobem práce s fotkami, možnostmi vyhledávání v nich podle obsahu, vykopírování textu z nich, provázaností s mapami, recenzemi, možnostmi sdílení, automatickým seskupováním a podobně.","feed_id":"5903","feed_title":"abclinuxu - blogy","flavor_image":"","flavor_stream":"","guid":"{\"ver\":2,\"uid\":\"52\",\"hash\":\"SHA1:1277107408b159882b95ca7151a0ec0160a3971a\"}","id":6939327,"is_updated":false,"labels":[],"lang":"","link":"http://www.abclinuxu.cz/blog/Co_to_je/2021/1/kam-s-fotkama","marked":false,"note":null,"published":false,"score":0,"tags":[""],"title":"Kam s fotkama?","unread":true,"updated":1609750800}
...
```

For RSS 2.0 message, the result might look as follows:
```
...
time="     3.568" type="debug" -> feed-downloader: Hooking message took 6 microseconds.
time="     3.568" type="debug" -> <item>
<title><![CDATA[Man Utd's Cavani 'not comfortable' in England, says father]]></title>
<description><![CDATA[Manchester United striker Edinson Cavani "does not feel comfortable" and could move back to his native South America, his father said.]]></description>
<link>https://www.bbc.co.uk/sport/football/56341983</link>
<guid isPermaLink="true">https://www.bbc.co.uk/sport/football/56341983</guid>
<pubDate>Tue, 09 Mar 2021 23:46:03 GMT</pubDate>
</item>

time="     3.568" type="debug" -> feed-downloader: Running filter script, it took 416 microseconds.
...
```

Write details of available labels and assign the first label to the message:
```js
function filterMessage() {
  console.log('Number of assigned labels: ' + msg.assignedLabels.length);
  console.log('Number of available labels: ' + msg.availableLabels.length);

  var i;
  for (i = 0; i < msg.availableLabels.length; i++) {
    var lbl = msg.availableLabels[i];

    console.log('Available label:');
    console.log('  Title: \'' + lbl.title + '\' ID: \'' + lbl.customId + '\'');
  }

  if (msg.availableLabels.length > 0) {
    console.log('Assigning first label to message...');
    msg.assignLabel(msg.availableLabels[0].customId);

    console.log('Number of assigned labels ' + msg.assignedLabels.length);
  }

  console.log();
  return MessageObject.Accept;
}
```

Make sure that your receive only one message/article with particular URL across all your feeds (from same plugin) and all other messages with same URL are subsequently ignored:
```js
function filterMessage() {
  if (msg.isAlreadyInDatabase(MessageObject.SameUrl | MessageObject.AllFeedsSameAccount)) {
    return MessageObject.Ignore;
  }
  else {
    return MessageObject.Accept;
  }
}
```

Remove "ads" from messages received from Inoreader. Method simply removes `div` which contains the advertisement:
```js
function filterMessage() {
  msg.contents = msg.contents.replace(/<div>\s*Ads[\S\s]+Remove<\/a>[\S\s]+adv\/www\/delivery[\S\s]+?<\/div>/im, '');

  return MessageObject.Accept;
}
```

### Websites Scraping <a id="scrap"></a>
> **Only proceed if you consider yourself a power user and you know what you are doing!**

RSS Guard offers additional advanced features inspired by [Liferea](https://lzone.de/liferea/).

You can select source type of each feed. If you select `URL`, then RSS Guard simply downloads feed file from given location and behaves like everyone would expect.

However, if you choose `Script` option, then you cannot provide URL of your feed and you rely on custom script to generate feed file and provide its contents to [standard output](https://en.wikipedia.org/wiki/Standard_streams#Standard_output_(stdout)). Data written to standard output should be valid feed file, for example RSS or ATOM XML file.

`Fetch it now` button also works with `Script` option. Therefore, if your source script and (optional) post-process script in cooperation deliver a valid feed file to the output, then all important metadata, like title or icon of the feed, can be discovered :sparkles: automagically :sparkles:.

<img alt="alt-img" src="images/scrape-source-type.png" width="350px">

Any errors in your script must be written to [error output](https://en.wikipedia.org/wiki/Standard_streams#Standard_error_(stderr)).

> **As of RSS Guard 4.2.0, you cannot separate your arguments with `#`. If your argument contains spaces, then enclose it with DOUBLE quotes, for example `"my argument"`. DO NOT use SINGLE quotes to do that.**

If everything goes well, script must return `0` as the process exit code, or a non-zero exit code if some error happened.

Executable file must be always be specified, while arguments not. Be very careful when quoting arguments. Some examples of valid and tested execution lines are:

| Command       | Explanation   |
| :---          | ---           |
| `bash -c "curl 'https://github.com/martinrotter.atom'"`   | Download ATOM feed file using Bash and Curl. |
| `Powershell Invoke-WebRequest 'https://github.com/martinrotter.atom' \| Select-Object -ExpandProperty Content` | Download ATOM feed file with Powershell. |
| `php tweeper.php -v 0 https://twitter.com/NSACareers` | Scrape Twitter RSS feed file with [Tweeper](https://git.ao2.it/tweeper.git). Tweeper is the utility which is able to produce RSS feed from Twitter and other similar social platforms. |

Note that the above examples are cross-platform and you can use the exact same command on Windows, Linux or macOS, if your operating system is properly configured.

This feature is very flexible and can be used to scrape data with [CSS selectors](https://www.w3schools.com/cssref/css_selectors.asp). There is ready-made [Python script](https://github.com/Owyn/CSS2RSS) which can be used to scrape websites with CSS selectors very easily. Make sure to give its author the credit he deserves.

RSS Guard offers [placeholder](#userd-plac) `%data%` which is automatically replaced with full path to RSS Guard's [user data folder](#userd), allowing you to make your configuration fully portable. You can, therefore, use something like this as source script line: `bash#%data%/scripts/download-feed.sh`.

Also, working directory of process executing the script is set to point to RSS Guard's user data folder.

There are some examples of website scrapers [here](https://github.com/martinrotter/rssguard/tree/master/resources/scripts/scrapers), most of them are written in Python 3, thus their execution line is similar to `python script.py`. Make sure to examine each script for more information on how to use it.

After your source feed data are downloaded either via URL or custom script, you can optionally post-process the data with one more custom script, which will take **raw source data as input** and must produce valid feed data to **standard output** while printing all error messages to **error output**.

Format of post-process script execution line is the same as above.

<img alt="alt-img" src="images/scrape-post.png" width="350px">

Typical post-processing filter might do things like advanced CSS formatting, localization of content to another language, downloading of full articles, some kind of filtering or removing ads.

It's completely up to you if you decide to only use script as `Source` of the script or separate your custom functionality between `Source` script and `Post-process` script. Sometimes you might need different `Source` scripts for different online sources and the same `Post-process` script and vice versa.

### Notifications <a id="notif"></a>
RSS Guard allows you to customize desktop notifications. There are a number of events which can be configured:
* New (unread) articles fetched.
* Fetching of articles is started.
* Login OAuth tokens are refreshed.
* New RSS Guard version is available.
* etc.

<img alt="alt-img" src="images/notif.png" width="600px">

Your notification can also play `.wav` sounds which you can place under your [user data folder](#userd) and use them via special [placeholder](#userd-plac). Other audio formats are not supported.

### Database Backends <a id="datab"></a>
RSS Guard offers switchable database backends which hold your data. At this point, two backends are available:
* MariaDB
* SQLite (default)

SQLite backend is very simple to use, no further configuration is needed and all your data is stored in single file
```
<user-data-root-folder>\database\local\database.db
```
Check `About RSS Guard -> Resources` dialog to find more info on significant paths used. This backend offers "in-memory" database option, which automatically copies all your data into RAM when application launches, and then works solely with that RAM data, which makes RSS Guard incredibly fast. Data is also written back to database file when application exits. Note that this option should be used very rarely because RSS Guard should be fast enough with classic SQLite persistent DB files. Only use this with a huge amount of article data, and when you know what you are doing.

MariaDB (MySQL) backend is there for users, who want to store their data in a centralized way. You can have single server in your network and use multiple RSS Guard instances to access the data.

For database-related configuration see `Settings -> Data storage` dialog.

### User Data Portability <a id="userd"></a>
One of the main goals of RSS Guard is to have local application data portable (relocatable) so that they can be use across all [supported operating systems](#sos).

RSS Guard is able to run in two modes:

* Default mode is *"non-portable"* mode, where user data folder is placed in user-wide "config directory" (this is `C:\Users\<user>\AppData\Local` on Windows). If subfolder with file
```
RSS Guard 4\data\config\config.ini
```
exists, then this user folder is used.

* The other mode is that user data folder is placed in subfolder `data4` in the same directory as RSS Guard binary (`rssguard.exe` on Windows). This *"portable"* mode is automatically enabled if "non-portable" mode detection fails.

User data folder can host your custom icon themes in `icons` subfolder and custom skins in `skins` subfolder.

#### `%data%` placeholder <a id="userd-plac"></a>
RSS Guard stores its data and settings in single folder. What exact folder it is is described [here](#portable-user-data). RSS Guard allows you to use the folder programmatically in some special contexts via `%data%` placeholder. You can use this placeholder in these RSS Guard contexts:
* Contents of your [article filters](#fltr) - you can, therefore, place some scripts under your user data folder and include them via `JavaScript` into your article filter.
* Contents of each file included in your custom [skins](#skin). Note that in this case, the semantics of `%data%` are little changed and `%data%` points directly to base folder of your skin.
* `source` and `post-process script` attributes of for [scraping](#scrap) feed - you can use the placeholder to load scripts to generate/process feed from user data folder.
* Notifications also support the placeholder in path to audio files which are to be played when some event happens. For example you could place audio files in your data folder and then use them in notification with `%data%\audio\new-messages.wav`. See more about notifications [here](#notif).

### Built-in Web Browser with AdBlock <a id="webb"></a>
RSS Guard is distributed in two variants:
* **Standard package with WebEngine-based bundled article viewer**: This variant displays messages/articles with their full formatting and layout in embedded Chromium-based web browser. This variant of RSS Guard should be nice for everyone. Also, installation packages are relatively big.

* **Lite package with simple text-based article viewer**: This variant displays article in much simpler and much more lightweight web viewer component. All packages of this variant have `nowebengine` keyword in their names. This variant of RSS Guard uses [litehtml](https://github.com/litehtml/litehtml) to render HTML/CSS layout of your articles. This component does NOT include JavaScript and is meant to be here for people who value their privacy.

#### AdBlock <a id="adbl"></a>
Both variants of RSS Guard offer ad-blocking functionality via [Adblocker](https://github.com/cliqz-oss/adblocker). Adblocker offers similar performance to [uBlock Origin](https://github.com/gorhill/uBlock).

If you want to use AdBlock, you need to have [Node.js](#node) installed.

You can find elaborate lists of AdBlock rules [here](https://easylist.to). You can just copy direct hyperlinks to those lists and paste them into the "Filter lists" text-box as shown below. Remember to always separate individual links with newlines. Same applies to "Custom filters", where you can insert individual filters, for example [filter](https://adblockplus.org/filter-cheatsheet) "idnes" to block all URLs with "idnes" in them.

<img alt="alt-img" src="images/adblock.png" width="350px">

The way ad-blocking internally works is that RSS Guard starts local `HTTP` browser which provides ad-blocking API, which is subsequently called by RSS Guard. There is some caching done in between, which speeds up some ad-blocking decisions.

## Minor Features <a id="mife"></a>

### Files Downloader <a id="downl"></a>
RSS Guard offers simple embedded file downloader.

<img alt="alt-img" src="images/downloader-window.png" width="600px">

You can right click on any item in embedded web browser and hit `Save as` button. RSS Guard will then automatically display the downloader, and will download your file. This feature works in [both RSS Guard variants](#webb).

<img alt="alt-img" src="images/downloader-view.png" width="600px">

You can download up to 6 files simultaneously.

### Node.js <a id="node"></a>
RSS Guard integrates [Node.js](https://nodejs.org). Go to `Node.js` section of `Settings` dialog to see more.

Node.js is used for some advanced functionality like [AdBlock](#adbl).


### Labels <a id="lbls"></a>
RSS Guard supports labels (tags). Any number of tags can be assigned to any article.

Note that tags in some plugins are [synchronizable](#sfrl). While labels are synchronized with these services, sometimes they cannot be directly created via RSS Guard. In this case, you have to create them via web interface of the respective service, and only after that perform `Synchronize folders & other items`, which will fetch newly created labels too.

Labels can be easily added via `Labels` root item.

<img alt="alt-img" src="images/label-menu.png" width="600px">

New label's title and color can also be chosen.

<img alt="alt-img" src="images/label-dialog.png" width="200px">

Unassigning a message label might easily be done through the message viewer.

<img alt="alt-img" src="images/label-assign.png" width="600px">

Note that unassigning a message labels is also synchronized at regular intervals (with services that support label synch).

Also, [message filters](#fltr) can assign or remove labels to/from messages.

### Skins <a id="skin"></a>
RSS Guard is a skinable application. Its GUI can be almost completely styled with [Qt stylesheets](https://doc.qt.io/qt-5/stylesheet.html).

<img alt="alt-img" src="images/gui-dark.png" width="600px">

> Note that as of RSS Guard 4.1.3, old skins `vergilius` and `dark` were removed and replaced with `nudus-*` skins which will be the only skins maintained by RSS Guard developers. There is skin "API" (described below) which is very extensive and allows to use many ways of tweaking the visual part of RSS Guard without much work.

You can select style and skin in settings category `User interface`.

Try to play with various combinations of styles and skins to achieve UI you like.

RSS Guard encapsulates styling capabilities via *skins* feature. Each skin is placed in its own folder and must contain several [files](https://github.com/martinrotter/rssguard/tree/master/resources/skins/plain). There are some [built-in](https://github.com/martinrotter/rssguard/tree/master/resources/skins) skins, but you can place your custom skins in your [user data folder](#userd). You can find exact path to your user data folder in `About` dialog. Note that there must be subfolder `skins`. Create it if it does not exist and place your custom skins inside.

<img alt="alt-img" src="images/about-skins.png" width="600px">

For example if your new skin is called `greenland`, you must place it in folder

```
<user-data-path>\skins\greenland
```

You can use the ["plain" skin](https://github.com/martinrotter/rssguard/tree/master/resources/skins/plain) as a reference for implementing your own skins. Go through its [README file](https://github.com/martinrotter/rssguard/tree/master/resources/skins/plain/README) for some advanced information.

As stated above, there are several files in each skin:
* `metadata.xml` - XML file with some basic information about the skin's name, author etc.
* `qt_style.qss` - [Qt stylesheet](https://doc.qt.io/qt-5/stylesheet.html) file.
* `html_*.html` - These are (partial) HTML files which are used by RSS Guard in various situations like displaying article or error page.

Note that not all skins have to provide full-blown theming for all UI components of RSS Guard. They can implement only some of the features, like provide custom HTML/CSS setup for article viewer and do some minimal Qt CSS styling for some UI controls.

To avoid confusion, when style `Fusion` is enabled, then option `Force dark look` becomes available. This option is completely independent from Qt stylesheet defined in skin's `qt_style.qss` file.

### GUI Tweaking <a id="guit"></a>
RSS Guard's main window appearance can be tweaked in many ways. You can hide menu, toolbars, status bar, you can also change orientation of article viewer to suit widescreen devices.

<img alt="alt-img" src="images/gui-hiding.png" width="600px">
<img alt="alt-img" src="images/gui-hiding-all.png" width="600px">
<img alt="alt-img" src="images/gui-layout-orientation.png" width="600px">
<img alt="alt-img" src="images/gui-dark.png" width="600px">
<img alt="alt-img" src="images/gui-dark2.png" width="600px">

### <a id="cli"></a>Command Line Interface
RSS Guard offers CLI (command line interface). For overview of its features, run `rssguard --help` in your terminal. You will see the overview of the interface.

```
Usage: rssguard [options] [url-1 ... url-n]
RSS Guard

Options:
  -h, --help                     Displays overview of CLI.
  -v, --version                  Displays version of the application.
  -l, --log <log-file>           Write application debug log to file. Note that
                                 logging to file may slow application down.
  -d, --data <user-data-folder>  Use custom folder for user data and disable
                                 single instance application mode.
  -s, --no-single-instance       Allow running of multiple application
                                 instances.
  -g, --no-debug-output          Disable just "debug" output.
  -n, --no-standard-output       Completely disable stdout/stderr outputs.
  -t, --style <style-name>       Force some application style.

Arguments:
  urls                           List of URL addresses pointing to individual
                                 online feeds which should be added.
```

RSS Guard can add feeds passed as URLs via command line arguments. Feed URI [scheme](https://en.wikipedia.org/wiki/Feed_URI_scheme) is supported, so that you can call RSS Guard like this:

```powershell
rssguard.exe "feed://archlinux.org/feeds/news"
rssguard.exe "feed:https//archlinux.org/feeds/news"
rssguard.exe "https://archlinux.org/feeds/news"
```

So in order to comfortably add feed directly to RSS Guard from you browser without copying its URL manually, you have to "open" RSS Guard "with" feed URL passed as parameter. There are [extensions](https://addons.mozilla.org/en-GB/firefox/addon/open-with/) which can do it.

## <a id="contrib"></a>For Contributors

### <a id="donat"></a>Donations
You can support author of RSS Guard via [donations](https://github.com/sponsors/martinrotter).

### <a id="compil"></a>Compiling RSS Guard
RSS Guard is a C++ application and all common build instructions can be found at the top of [CMakeLists.txt](https://github.com/martinrotter/rssguard/blob/master/CMakeLists.txt). Here's a quick example of how to build on Linux:

```bash
# Create a build directory.
mkdir build-dir
# Configure the project to build using Qt 6, and disable built-in web browser support.
cmake -B build-dir -S . -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_WITH_QT6=ON -DUSE_WEBENGINE=OFF
# Compile it, in parallel mode.
cmake --build build-dir -j$(nproc)
# (Optional) Run the build to test it.
./build-dir/src/rssguard/rssguard
# (Optional) Install RSS Guard system-wide.
sudo make -C build-dir install
```

### <a id="papi"></a>Plugin API
RSS Guard offers simple C++ API for creating new service plugins. All base API classes are in folder [`abstract`](https://github.com/martinrotter/rssguard/tree/master/src/librssguard/services/abstract). User must subclass and implement all interface classes:

| Class                 | Purpose   |
| :---                  | ---       |
| `ServiceEntryPoint`   | Very base class which provides basic information about the plugin name, author, etc. It also provides methods which are called when new account should be created and when existing accounts should be loaded from database. |
| `ServiceRoot`         | This is the core "account" class which represents an account node in feed's list, and offers interface for all critical functionality of a plugin, including handlers which are being called with a plugin's start/stop, marking messages as read/unread/starred/deleted, unassigning labels, etc. |

API is reasonably simple to understand but relatively large. Sane default behavior is employed where it makes sense.

Perhaps the best approach to use when writing new plugin is to copy [existing](https://github.com/martinrotter/rssguard/tree/master/src/librssguard/services/greader) one and start from there.

Note that RSS Guard can support loading of plugins from external libraries (`.dll`, `.so`, etc.) but the functionality must be polished because so far all plugins are directly bundled into the application as no one really requested run-time loading of plugins so far.

### <a id="reprt"></a>Reporting Bugs or Feature Requests
Please report all issues/bugs/ideas to [Issues](https://github.com/martinrotter/rssguard/issues) section. Describe your problem as precisely as possible, along with steps taken leading up to the issue occurring.

If you report any bug, you must provide application debug log. So make sure to start RSS Guard from command line (`cmd.exe` on Windows) with `--log` switch and path where you want to store log file, for example `rssguard.exe --log '.\rssguard.log'` which will save log file into your RSS Guard folder. After you've started RSS Guard this way, then reproduce your problem and upload log file to the ticket.

Also, for some broader questions or general ideas, use [discussions](https://github.com/martinrotter/rssguard/discussions) rather than [issues](https://github.com/martinrotter/rssguard/issues).

### <a id="locali"></a>Localization
RSS Guard currently includes [many localizations](http://www.transifex.com/projects/p/rssguard).

If you are interested in creating translations for RSS Guard, then do this:
1. Go [here](http://www.transifex.com/projects/p/rssguard) and check status of currently supported localizations.
2. [Login](http://www.transifex.com/signin) (you can use social networks to login) and work on existing translations. If no translation team for your country/language exists, then ask for creating of localization team via the website.

**All translators commit themselves to keep their translations up-to-date. If some translations are not updated by their authors regularly, and only a small number of strings is translated then those translations along with their teams will eventually be REMOVED from the project!!! At least 50% of strings must be translated for translation to be added to project.**

### <a id="migratt"></a>Migrating data
RSS Guard automatically migrates all your [user data](#userd) if you install newer minor version, for example if you update from `3.7.5` to `3.9.1`.

If you decide to upgrade to new major version, for example from `3.x.x` to `4.x.x`, then you cannot use your existing user data as major versions are declared as backwards incompatible, so such data transition are not supported.

### Migrating user data from `3.9.2` to `4.x.x`
> Only proceed if you consider yourself to be a SQL power user and you know what you are doing!
>
> Also, make sure that last RSS Guard from `3.x.x` line you used with your data was the most up-to-date `3.9.2` version.

Here is short DIY manual on how to manually update your `database.db` file to `4.x.x` format. Similar approach can be taken if you use `MariaDB` [database backend](#datab).

Here are SQLs for [old](https://github.com/martinrotter/rssguard/blob/3.9.2/resources/sql/db_init_sqlite.sql) schema and [new](https://github.com/martinrotter/rssguard/blob/4.0.0/resources/sql/db_init_sqlite.sql) schema.

### Converting `*Accounts` tables
***
In `3.x.x` each plugin/account type had its own table where it kept your login usernames, service URLs etc. In `4.x.x` all plugins share one table `Accounts` and place account-specific data into `custom_data` column. You simply can take all rows from any `*Accounts` table (for example `TtRssAccounts`) and insert them into `Accounts`, keeping all columns their default values, except of `type`, which must have one of these values:
* `std-rss` - for standard RSS/ATOM feeds.
* `tt-rss` - for Tiny Tiny RSS.
* `owncloud` - for Nextcloud News.
* `greader` - For all Google Reader API services, including Inoreader.
* `feedly` - for Feedly.
* `gmail` - for Gmail.

Then you need to go to `Edit` dialog of your account in RSS Guard (once you complete this migration guide) and check for all missing login information etc.

<a id="accid"></a>Also, once you add any rows the `Accounts` table, your row will be assigned unique `id` value which is integer and is used as foreign key in other DB tables, via column `account_id`.

### Converting `Feeds` table
***
There are some changes in `Feeds` table:
* `url` column is now named `source`,
* `source_type`, `post_process`, `encoding`, `type`, `protected`, `username`, `password` columns are removed and their data are now stored in JSON-serialized form in new column `custom_data`. Here is sample value of `custom_data`:

```json
{
  "encoding": "UTF-8",
  "password": "AwUujeO2efOgYpX3g1/zoOTp9JULcLTZzwfY",
  "post_process": "",
  "protected": false,
  "source_type": 0,
  "type": 3,
  "username": ""
}
```

Pay attention to `account_id` column as this column is the ID of your account as stated in the above [section](#accid).

### Converting `Messages` table
***
Columns were reordered and other than that new column `score` with sane default value was added. Therefore you can simply copy your data in a column-to-column mode.

Pay attention to `account_id` column as this column is the ID of your account as stated in the above [section](#accid).

### Other tables
***
Other tables like `Labels` or `MessageFilters` are unchanged between these two major RSS Guard versions. But you might need to adjust `account_id` to match DB ID of your account.
