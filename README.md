### sshd 错误登录日志上报

将 sshd 错误登录日志上报到集中的URL


### 下载和编译

```
cd /usr/src/
git clone https://github.com/bg6cq/sshdlogreport
cd sshdlogreport
make
```

### 运行

默认运行参数如下：

```
APIKEY文件: apikey.txt
上报URL: http://blackip.ustc.edu.cn/sshdlogreport.php
日志路径：/var/log/auth.log
```

以上参数可以通过命令行修改

如果使用默认参数，只要建立 apikey.txt 文件，内填上APIKEY即可。

第一次使用，可以使用命令行
```
./sshdlogreport -d 调试模式
```

之后正式运行，使用命令行
```
./sshdlogreport 即可
```
