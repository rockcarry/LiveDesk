LiveDesk 是一个 windows 桌面直播程序

目前支持以下功能：

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



rockcarry
2020-11-30

