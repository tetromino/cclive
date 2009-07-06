#!/bin/sh

# NOTE: Run mingw32-build.sh first. You may also want to comment
# out the last line "sigre ...".

# Create cclive-$release.zip package.

# Edit these as needed:
CURL_PATH="/usr/home/legatvs/src/curl-7.19.4"
CURL_DLL="$CURL_PATH/dist/bin/libcurl-4.dll"
CURL_COPYING="$CURL_PATH/COPYING"

VERSION=`awk '/PACKAGE_VERSION = / {print $3}' Makefile`
rm -rf dist cclive-$VERSION cclive-$VERSION.zip

make install-strip \
&& make man \
&& cp $CURL_DLL dist/bin \
&& cp COPYING dist \
&& cp $CURL_COPYING dist/COPYING.curl \
&& cp README README.w32 dist \
&& cp cclive.1.html dist \
&& rm -r dist/share \
&& mv dist/ cclive-$VERSION/ \
&& zip -9 -r cclive-$VERSION.zip cclive-$VERSION \
&& sigre-conf cclive-$VERSION.zip

exit $?

