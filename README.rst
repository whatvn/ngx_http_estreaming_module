
# **nginx_http_estreaming_module**
=================================
Nginx module that automatically:
    - generate hls (HTTP live streaming) m3u8 playlist with built-in adaptive bitrate support
    - split h264 file into small chunks *ts* files according to user's request video bitrate
    - play mp4 file (copy from ngx_http_mp4_module) using progressive download.
    - return video length if user request http://domain.com/streaming/video.len (video name is video.mp4).
    - Generate playlist which point ts file to CDN server if *hls_proxy* is configured
    - Can fix video to enable fast-start (quicktime faststart) streaming 


# demo
=====

http://ethicconsultant.com/player/ 

This is 720p video in h264 format without any pre-transcoding or segmenting.
Depend on your bandwidth, video will be transcoded and delivered on-the-fly. 

Please note: this server is a tiny server: 1core cpu with 512mb ram



# **detail** 
============

- This module is `ngx-hls-module` fork (which is also a fork from ngx_h264_module of *codeshop*). ngx_hls_module already supports generate hls playlist and split mp4 file on-the-fly. 
- ngx_http_estreaming_module extends ngx_hls_module to support adaptive bitrate, generate playlist based on bitrate/resolution of source video (eq: if source video has resolution 1280x720, nginx_http_estreaming_module with generate playlist with: 1280x720, 854x480, 640x360).
Then if user requests for 480p playlist, ts file will be transcoded to 480p and then response to client. 
- ngx_http_estreaming makes use of ffmpeg libraries: libavcodec, libavfilter, libavformat, libavresample. it also use libx264 to encode h264 video, and libfdk_acc to encode aac audio,  
- This module is a very expensive cpu usage module. it splits video into small chunk then transcode video on-the-fly. But it's faster than almost  current pre-transcoding solution. 
    
- I've tried so hard to optimize decoding/transcoding video process to make it fast, but if someone have experience on this, please give some help.

# **usage:**
===========

1. To compile:

1.1 You have to compile libx264, fdk-aac and ffmpeg libraries. Command I use to compile ffmpeg:

*fdk-aac on CentOS*
::

    # git clone https://github.com/mstorsjo/fdk-aac 
    # cd fdk-aac
    # ./configure ; make; make install 
    # yum install ffmpeg-devel x264-devel 



or compile everything like I do 

::
    
    ./configure --enable-libx264 --enable-static --disable-opencl --extra-ldflags='-L/usr/local/lib -lx264 -lpthread -lm' --enable-gpl --enable-libfdk-aac --enable-nonfree
    make 
    make install 


1.2 Compile nginx with --add-module option

::
    
    ./configure --add-module=ngx_http_estreaming_module 



2. To use:

This module take various request's query string to determine which playlist, resolution should be return to client request, and because using query strings makes url looks dynamic, it's not caching friendly,
this module then pre-rewrite url when writing output playlist, but parsing url without using query string is hard and takes too much time, so it still using query-strings when processing input-url.
We have to use rewrite module here

::    

    #uncomment to use secure_link module
    #secure_link           $arg_st,$arg_e;
    #secure_link_md5       "axcDxSVnsGkAKvqhqOh$host$arg_e";
    #                       if ($secure_link = "")  { return 403; }
    #               if ($secure_link = "0") { return 410; }
    rewrite ^(.*)/(adbr)/([0-9]+p)/([0-9]+)/(.*ts)?(.*) $1/$5?video=$4&$2=true&vr=$3&$6 last;
    rewrite ^(.*)/(adbr)/([0-9]+p)/(.*\.m3u8)?(.*) $1/$4?$2=true&vr=$3&$5 last;
    rewrite ^(.*)/(org)/(.*\.m3u8)?(.*) $1/$3?$2=true&$6 last;
    rewrite ^(.*)/(org)/([0-9]+)/(.*\.ts)?(.*) $1/$4?video=$3&$2=true&$5 last;
    rewrite ^(.*)/([0-9]+)/(.*ts)?(.*)  $1/$3?video=$2&$4 last;
    location /upload {
            streaming;
            error_log /data/log/error.log ;
            root   /data/demo;
            segment_length 5;
            hls_buffer_size 1m;
            hls_max_buffer_size 50m;
            mp4_buffer_size 1m;
            mp4_max_buffer_size 500m;
        }



With these configuration, for example your domain name is: streaming.domain.com, your root directory is `/data/demo`, you upload a video named `demo.mp4`, in sub-directory `upload` with resolution 1280x720, to play it, just point your player to url:

::

    http://streaming.domain.com/upload/demo.m3u8

this module will take care all the rest, generate adaptive playlist, transcode video to requested resolution. For more information about http live streaming, just read: http://en.wikipedia.org/wiki/HTTP_Live_Streaming
Example output `demo.m3u8`:

::

    #EXTM3U
    #EXT-X-ALLOW-CACHE:NO
    #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1280000, RESOLUTION=640x360,CODECS="mp4a.40.2, avc1.4d4015"
    adbr/360p/demo.m3u8
    #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=2560000, RESOLUTION=854x480,CODECS="mp4a.40.2, avc1.4d4015"
    adbr/480p/demo.m3u8
    #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=5120000, RESOLUTION=1280x720,CODECS="mp4a.40.2, avc1.4d4015"
    org/demo.m3u8


When receive master playlist, player will choose which child playlist should be use at current time according to local bandwidth. For example, if local bandwidth is fine, player will use HD resolution which is `#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=5120000, RESOLUTION=1280x720`, example content of this playlist:

::

    #EXTM3U
    #EXT-X-TARGETDURATION:8
    #EXT-X-MEDIA-SEQUENCE:0
    #EXT-X-VERSION:4
    #EXTINF:6.001,
    0/demo.ts
    #EXTINF:6.001,
    3/demo.ts
    #EXTINF:6.001,
    6/demo.ts
    #EXTINF:0.200,
    9/demo.ts
    #EXT-X-ENDLIST


This module was tested with: jwplayer, html5, flowplayer, flashhls, ios device, Mac OS, and new android version... 



# **directive:**
===============

- *streaming* : enable this module in server|location 
- *segment_length* : length of ts chunk files, in second
- *hls_buffer_size*: size in b/k/m/g size of hls moov atom buffer (usually 500 kB is enough)
- *hls_max_buffer_size* : size in b/k/m/g max size of hls moov atom buffer size
- *mp4_buffer_size*: size in b/k/m/g size of mp4 moov atom buffer - from original ngx_http_mp4_module (usually 500 kB is enough)
- *mp4_max_buffer_size*: size in b/k/m/g max size of mp4 moov atom buffer - from original ngx_http_mp4_module
- *hls_proxy_address*: string when this directive is configured, instead of generate playlist with relative ts url, a full url will be produced: /adbr/360p/12/demo.ts -> http://cdn.stream.domain.com/adbr/360p/12/demo.ts
- *fix_mp4*: on|of In order to split mp4 quickly, mp4 file shoule be encode using 2-pass encoding, or using a tool to move moov-atom data to the beginning of mp4 file. If this flag is enable, mp4 file will be fix automatically. 



# **roadmap**
=============

1. support Http dynamic streaming (HDS)
2. support other video extension: mkv, avi, flv...
3. make use of nginx event 
4. optimize transcoding process to make it faster 
5. support hls encryption.


# **Note**
==========
If you use this module, you don't have to use ngx_http_mp4_module anymore, since it already embeded into this module.



# **license** 
=============    
Because this module based on ngx_h264_module from codeshop, you should consider their license. It also use libx264, and x264 uses GPLv2, so this module also uses GPLv2 too.

    
