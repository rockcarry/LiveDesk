LiveDesk 是一个 windows 桌面直播程序

目前支持以下功能
----------------

1. 基于 live555 的 rtsp 服务器
2. 基于 librtmp 的 rtmp 推流功能
3. 桌面录制并保存为 mp4 视频文件
4. avkcp 协议的直播功能
5. ffrdp 协议的直播功能


音频数据从 wavein 获取
视频数据从 gdi    获取

音频编码采用 g711a 或 aac 编码
视频编码采用 x264 编码


avkcp 是基于 kcp 协议实现的音视频传输，可直接使用 fanplayer 播放 avkcp 的码流
ffrdp 是我基于我自己开发的 ffrdp 协议实现的音视频传输，需要使用 fanplayer 播放
ffrdp 协议目前已经优化的比较稳定，性能应该不差于 kcp，并且目前支持 fec 和自适应码率，在实时音视频直播上有更好的性能和体验



livedesk 使用说明
-----------------

支持的参数：
--aac            使用 aac 音频编码，不指定 -aac 将采用 pcm alaw 编码
--channels=N     指定音频通道数
--samplerate=xxx 指定音频采样率
--abitrate=xxx   指定音频编码码率
--vwidth=xxx     指定视频宽度
--vheight=xxx    指定视频高度
--framerate=xx   指定视频帧率
--vbitrate=xxx   指定视频编码码率
--rtsp=xxxx      使用 rtsp 服务器直播
--avkcps=xxxx    使用 avkcps 服务器直播，xxxx 为端口号
--ffrdps=xxxx    使用 ffrdps 服务器直播，xxxx 为端口号
--rtmp=url       使用 rtmp 推流直播
--mp4=filename   屏幕录制保存到 .mp4 文件
--duration=xxx   指定录像分段时长 ms 为单位

程序运行后支持的命令：
- help: show this mesage.
- quit: quit this program.
- mp4_start  : start recording screen to mp4 files.
- mp4_pause  : pause recording screen to mp4 files.
- rtmp_start : start rtmp push.
- rtmp_pause : pause rtmp push.
- ffrdps_dump: dump ffrdps server.

命令行参数示例：
LiveDesk --aac --channels=2 --samplerate=48000 --abitrate=128000 --vbitrate=2560000 --mp4=test


rockcarry
2020-11-30

