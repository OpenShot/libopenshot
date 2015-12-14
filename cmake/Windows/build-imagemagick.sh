#!/bin/bash
# xml2 build ok but failed test
# libfpx build error

function ised() {
    IN=$1
    shift
    tmp=$RANDOM.$$
    <$IN sed "$@" >$tmp && cat $tmp > $IN
    rm $tmp
}

function ask() {
    read -p "${1:-Are you sure?]} [Y/n] " response
    case $response in
        y|Y|"")
            true;;
        *)
            false;;
    esac
}

function download() {
    while IFS=\; read url md5 <&3; do
        fileName=${url##*/}

        echo "Downloading ${fileName}..."
        while true; do
            if [[ ! -e $fileName ]]; then
                wget ${url} -O ${fileName}
            else
                echo "File exists!"
            fi

            localMd5=$(md5sum ${fileName} | cut -d\  -f1)

            if [[ ${localMd5} != ${md5} ]]; then
                ask "Checksum failed. Do you want to download this file again? [Y/n] "
                if [[ $? -ne 0 ]]; then
                    exit 1
                fi
                rm ${fileName}
            else
                break
            fi
        done
    done 3< urls.txt
}

function extract() {
    file=$1
    if [[ ! -e ${file} ]]; then
        return
    fi

    case $file in
        *.tar.gz)
            tar xzf $file
            ;;
        *.tar.xz|*.tar.lzma)
            tar xJf $file
            ;;
        *.tar.bz2)
            tar xjf $file
            ;;
        *)
            "Don't know how to extract $file"
    esac
}

function isLibsInstalled() {
    libs="$@"
    notfound=false
    for l in "${libs}"; do
        ld -L/usr/local/lib -l"${l}" 2>/dev/null
        if [[ $? -ne 0 ]]; then
            notfound=true
        fi
    done

    ! ${notfound}
}

function isDirExists() {
    dir="$@"
    found=false
    for d in ${dir}; do
        if [[ -d "${d}" ]]; then
            found=true
            break
        fi
    done

    ${found}
}

function extractIfNeeded() {
    file=$1
    isDirExists ${file%%-*}-*
    if [[ $? -ne 0 ]]; then
        echo "Extracting $file"
        extract $file
    fi
}

function buildbzip2() {
    if isLibsInstalled "bz2"; then
        if ask "Found bzip2 installed. Do you want to reinstall it?"; then :
        else
            return 0
        fi
    fi

    extractIfNeeded bzip2-*.tar.lzma

    cd bzip2-*/
    tar xzf bzip2-1.0.6.tar.gz
    tar xzf cygming-autotools-buildfiles.tar.gz
    cd bzip2-*/
    autoconf
    mkdir ../build
    cd ../build
    ../bzip2-*/configure
    make
    make install
    cd ../..
}

function buildzlib() {
    if isLibsInstalled "z"; then
        if ask "Found zlib installed. Do you want to reinstall it?"; then :
        else
            return 0
        fi
    fi

    extractIfNeeded zlib-*.tar.xz

    cd zlib-*/
    INCLUDE_PATH=/usr/local/include LIBRARY_PATH=/usr/local/lib BINARY_PATH=/usr/local/bin make install -f win32/Makefile.gcc SHARED_MODE=1
    cd ..
}

function buildlibxml2() {
    if isLibsInstalled "xml2"; then
        if ask "Found libxml2 installed. Do you want to reinstall it?"; then :
        else
            return 0
        fi
    fi
    extractIfNeeded libxml2-*.tar.gz
    cd libxml2-*/win32/
    ised configure.js 's/dirSep = "\\\\";/dirSep = "\/";/'
    cscript.exe configure.js compiler=mingw prefix=/usr/local
    # ised ../dict.c '/typedef.*uint32_t;$/d'
    ised Makefile.mingw 's/cmd.exe \/C "\?if not exist \(.*\) mkdir \1"\?/mkdir -p \1/'
    ised Makefile.mingw 's/cmd.exe \/C "copy\(.*\)"/cp\1/'
    ised Makefile.mingw '/cp/{y/\\/\//;}'
    ised Makefile.mingw '/PREFIX/{y/\\/\//;}'
    make -f Makefile.mingw
    make -f Makefile.mingw install
    cd ../../
}

function buildlibpng() {
    if isLibsInstalled "png"; then
        if ask "Found libpng installed. Do you want to reinstall it?"; then :
        else
            return 0
        fi
    fi

    extractIfNeeded libpng-*.tar.xz

    cd libpng-*/
    make -f scripts/makefile.msys
    make install -f scripts/makefile.msys
    cd ..
}

function buildjpegsrc() {
    if isLibsInstalled "jpeg"; then
        if ask "Found jpegsrc installed. Do you want to reinstall it?"; then :
        else
            return 0
        fi
    fi

    extract jpegsrc*.tar.gz

    cd jpeg-*/
    ./configure
    make
    make install
    cd ..
}

function buildfreetype() {
    if isLibsInstalled "freetype"; then
        if ask "Found freetype installed. Do you want to reinstall it?"; then :
        else
            return 0
        fi
    fi
    extract freetype*.tar.bz2

    INCLUDE_PATH=/usr/local/include
	LIBRARY_PATH=/usr/local/lib
	BINARY_PATH=/usr/local/bin
    cd freetype-*/
    ./configure
	make
    make install
    cd ..
}

function buildlibwmf() {
    if isLibsInstalled "wmf"; then
        if ask "Found libwmf installed. Do you want to reinstall it?"; then :
        else
            return 0
        fi
    fi
    extract libwmf*.tar.gz

    cd libwmf-*/
    ./configure CFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"
    make
    make install
    cd ..
}

function buildlibwebp() {
    if isLibsInstalled "webp"; then
        if ask "Found libwebp installed. Do you want to reinstall it?"; then :
        else
            return 0
        fi
    fi
    extract libwebp*.tar.gz

    cd libwebp-*/
    ./configure CFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"
    make
    make install
    cd ..
}

function buildDelegate() {
    delegates="bzip2 zlib libxml2 libpng jpegsrc freetype libwmf libwebp"
    for d in ${delegates}; do
        echo "**********************************************************"
        echo "Building $d"
        build${d}
    done
}

function build() {
    extractIfNeeded ImageMagick-*.tar.xz

    local oldPwd=$(pwd -L)
    cd ImageMagick-*/
    # patch configure
    #sed -i 's/${GDI32_LIBS}x" !=/${GDI32_LIBS} ==/' configure
    ised configure 's/${GDI32_LIBS}x" !=/${GDI32_LIBS} ==/'
    ./configure --enable-shared --disable-static --enable-delegate-build --without-modules CFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"
    make
    make install
    cd ${oldPwd}
}

download
buildDelegate
build
