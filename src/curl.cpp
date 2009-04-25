/*
 * cclive Copyright (C) 2009 Toni Gundogdu. This file is part of cclive.
 * 
 * cclive is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 * 
 * cclive is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>

#include "singleton.h"
#include "cmdline.h"
#include "opts.h"
#include "except.h"
#include "macros.h"
#include "video.h"
#include "progressbar.h"
#include "macros.h"
#include "curl.h"

static CURL *curl;
static char curlErrorBuffer[CURL_ERROR_SIZE];

CurlMgr::CurlMgr()
{
    curl = NULL;
    curlErrorBuffer[0] = '\0';
}

    // Keeps -Weffc++ happy.
CurlMgr::CurlMgr(const CurlMgr&)
{
    curl = NULL;
    curlErrorBuffer[0] = '\0';
}

    // Ditto.
CurlMgr&
CurlMgr::operator=(const CurlMgr&) {
    return *this;
}

CurlMgr::~CurlMgr() {
    curl_easy_cleanup(curl);
    curl = NULL;
}

void
CurlMgr::init() {
    curl = curl_easy_init();
    if (!curl)
        throw RuntimeException("error: curl_easy_init returned NULL\n");

    Options opts = optsmgr.getOptions();

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, opts.agent_arg);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, opts.debug_given);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrorBuffer);

    char *proxy = opts.proxy_arg;
    if (opts.no_proxy_given)
        proxy = const_cast<char*>("");

    curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
}

struct mem_s {
    mem_s() : content("") { }
    std::string content;
};

static size_t
callback_writemem(void *p, size_t size, size_t nmemb, void *data) {
    mem_s *m    = reinterpret_cast<mem_s *>(data);
    m->content += reinterpret_cast<char*>(p);
    return size * nmemb;
}

std::string
CurlMgr::fetchToMem(const std::string& url, const std::string &what) {
    std::cout << "fetch ";
    if (what.empty())
        std::cout << url;
    else
        std::cout << what;
    std::cout << " ..." << std::flush;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_ENCODING, "");

    mem_s mem;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback_writemem);

    Options opts = optsmgr.getOptions();

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
        opts.connect_timeout_arg);

    /*
     * http://curl.haxx.se/docs/knownbugs.html:
     * 
     * "When connecting to a SOCKS proxy, the (connect) timeout is
     * not properly acknowledged after the actual TCP connect (during
     * the SOCKS 'negotiate' phase)."
     */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,
        opts.connect_timeout_socks_arg);

    curlErrorBuffer[0] = '\0';
    CURLcode rc = curl_easy_perform(curl);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);

    std::string errmsg;

    if (CURLE_OK == rc) {
        long httpcode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
        if (httpcode == 200)
            std::cout << "done." << std::endl;
        else
            errmsg = "server returned http/"+httpcode;
    }
    else
        errmsg = curlErrorBuffer;

    curlErrorBuffer[0] = '\0';

    if (!errmsg.empty())
        throw FetchException(errmsg);

    return mem.content;
}

void
CurlMgr::queryFileLength(VideoProperties& props) {
    FILE *f = tmpfile();
    if (!f) {
        perror("tmpfile");
        throw RuntimeException("error: tmpfile(3) failed");
    }
    std::cout << "verify video link ..." << std::flush;

    Options opts = optsmgr.getOptions();

    curl_easy_setopt(curl, CURLOPT_URL, props.getLink().c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);      // GET -> HEAD
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
                     opts.connect_timeout_arg);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                     opts.connect_timeout_socks_arg);

    curlErrorBuffer[0] = '\0';
    CURLcode rc = curl_easy_perform(curl);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); // reset HEAD -> GET

    fflush(f);
    fclose(f);

    long httpcode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);

    double len = 0;
    char *ct = NULL;

    std::string errmsg;

    if (rc == CURLE_OK && httpcode == 200) {
        rc = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);

        if (CURLE_OK == rc && NULL != ct)
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &len);

        if (CURLE_OK == rc && NULL != ct && len > 0) {
            props.setLength(len);
            props.setContentType(ct);
            std::cout << "done." << std::endl;
        }
        else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
            errmsg = "server returned http/"+httpcode;
        }
    }
    else {
        if (strlen(curlErrorBuffer) > 0)
            errmsg = curlErrorBuffer;
        else
            errmsg = "server returned http/"+httpcode;
    }

    curlErrorBuffer[0] = '\0';

    if (!errmsg.empty())
        throw FetchException(errmsg);
}

struct write_s {
    write_s()
        : filename(NULL), initial(0), file(NULL) { }
    char *filename;
    double initial;
    FILE *file;
};

static size_t
callback_writefile(void *data, size_t size, size_t nmemb, void *p) {
    write_s *write = reinterpret_cast<write_s*>(p);
    if (NULL != write && !write->file) {
        const char *mode = write->initial ? "ab" : "wb";
        write->file = fopen(write->filename, mode);
        if (!write->file) {
            perror("fopen");
            return -1;
        }
    }
    return fwrite(data, size, nmemb, write->file);
}

int
callback_progress(
    void *p,
    double total,
    double now,
    double utotal,
    double unow)
{
    ProgressBar *pb = static_cast<ProgressBar*>(p);
    pb->update(now);
    return 0;
}

void
CurlMgr::fetchToFile(VideoProperties& props) {
    Options opts = optsmgr.getOptions();

    double initial = props.getInitial();

    if (opts.continue_given && initial > 0) {
        double remaining = props.getLength() - initial;
        std::cout
            << "from: "
            << std::setprecision(0)
            << initial
            << " ("
            << std::setprecision(1)
            << _TOMB(initial)
            << "M)  remaining: "
            << std::setprecision(0)
            << remaining
            << " ("
            << std::setprecision(1)
            << _TOMB(remaining)
            << "M)"
            << std::endl;
    }

    curl_easy_setopt(curl, CURLOPT_URL, props.getLink().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback_writefile);

    write_s write;
    memset(&write, 0, sizeof(write));

    write.initial  = initial;
    write.filename = const_cast<char*>(props.getFilename().c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, &callback_progress);

    ProgressBar pb;
    pb.init(props);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &pb);

    curl_easy_setopt(curl, CURLOPT_ENCODING, "identity");
    curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM, (long)initial);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
                     opts.connect_timeout_arg);

    curl_off_t limit_rate = opts.limit_rate_arg * 1024;
    curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, limit_rate);

    curlErrorBuffer[0] = '\0';
    CURLcode rc = curl_easy_perform(curl);

    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM, 0L);
    curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t) 0);

    std::string errmsg;

    if (CURLE_OK != rc) {
        long httpcode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);

        if (strlen(curlErrorBuffer) > 0)
            errmsg = curlErrorBuffer;
        else
            errmsg = "server closed with http/"+httpcode;
    }

    curlErrorBuffer[0] = '\0';

    if (NULL != write.file) {
        fflush(write.file);
        fclose(write.file);
    }

    if (!errmsg.empty())
        throw FetchException(errmsg);

    pb.finish();

    std::cout << std::endl;
}

const std::string&
CurlMgr::unescape(std::string& url) const {
    char *p = curl_easy_unescape(curl, url.c_str(), 0, 0);
    url     = p;
    _FREE(p);
    return url;
}

void
CurlMgr::logIntoYoutube() {
    Options opts = optsmgr.getOptions();

    std::stringstream b;
    std::string passw;

    if (!opts.youtube_pass_given) {
        b   << "Login password for "
            << opts.youtube_user_arg
            << ":"
            << std::flush;
        passw = getpass(b.str().c_str());
    }
    else
        passw = opts.youtube_pass_arg;

    std::stringstream req;
    req << "http://uk.youtube.com/login?current_form=loginform"
        << "&username=" << opts.youtube_user_arg
        << "&password=" << passw
        << "&action_login=log+in&hl=en-GB";

    passw.clear();

    curl_easy_setopt(curl, CURLOPT_URL, req.str().c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback_writemem);

    mem_s mem;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
                     opts.connect_timeout_arg);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                     opts.connect_timeout_socks_arg);

    std::cout
        << "[youtube] login as "
        << opts.youtube_user_arg
        << "..."
        << std::flush;

    CURLcode rc = curl_easy_perform(curl);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);

    if (CURLE_OK == rc) {
        long    httpcode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
        if (httpcode == 200) {
            if (mem.content.find(
                "your log-in was incorrect") != std::string::npos)
            {
                throw RuntimeException("login was incorrect");
            }
            else if(mem.content.find(
                "check your password") != std::string::npos)
            {
                throw RuntimeException("check your login password");
            }
            else if (mem.content.find(
                "too many login failures") != std::string::npos)
            {
                throw RuntimeException("too many login failures, try later");
            }
            else {
                curl_easy_setopt(curl, CURLOPT_COOKIE, "is_adult=1");
                std::cout << "done." << std::endl;
            }
        }
        else
            throw RuntimeException("server returned http/"+httpcode);
    }
}

CurlMgr::FetchException::FetchException(const std::string& error)
    : RuntimeException(error)
{
}