#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <termio.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <gd.h>
#include <math.h>

gdImagePtr renderImage;
gdImagePtr outputImage;

int bg;
int shadow;
int text;
int doRespawn = 0;
char *textFont = "/home/matt/.fonts/Hack-Bold.ttf";
int textFontSize = 20;
char *digitFont = "/home/matt/.fonts/TickingTimebombBB.ttf";
int digitFontSize = 120;
int digitSpace = 72;
int digitDotSpace = 20;
int lineStart = 100;
int lineSize = 30;
int lineColor;

const uint32_t width = 512;
const uint32_t height = 512;

typedef enum {
    CLOSED,
    OPENING,
    OPEN,
    CLOSING
} state_t;

double displayPos = 0;

state_t state = CLOSED;

#define MCAST_IP "224.0.180.69"

int doShadow = 0;

uint8_t *buffer;


const uint16_t port = 6502;
uint32_t deviceId = 0;

char *mcastIp="0.0.0.0";

char *title = NULL;

#define DEVICE_ACTIVE 0x00000001

#define CLASS_PSU_V     0x00000001
#define CLASS_PSU_A     0x00000002
#define CLASS_PSU_VA    0x00000003

struct packet {
    uint32_t deviceId;
    uint32_t deviceClass;
    uint32_t flags;
    int32_t ma[10];
    int32_t mv[10];
};

struct packet data;

void service(int sock) {
    ssize_t rv;
    socklen_t client_len;
    struct sockaddr_in client_addr;
    struct packet incoming;

    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    tv.tv_sec = 0;
    tv.tv_usec = 1000;

    retval = select(sock+1, &rfds, NULL, NULL, &tv);

    if (retval) {

        bzero((char *)&incoming, sizeof(incoming));
        bzero((char *)&client_addr, sizeof(struct sockaddr_in));
        client_len = sizeof(struct sockaddr_in);

        rv = recvfrom(sock, (char *)&incoming, sizeof(incoming), 0, (struct sockaddr *)&client_addr, &client_len);

        if (rv != -1) {
            int i;
            if (incoming.deviceId == deviceId) {
                memcpy(&data, &incoming, sizeof(incoming));
            }
        }
    }
}

int stringWidth(char *txt) {
    int brect[8];
    gdImageStringFT(renderImage, brect, text, textFont, textFontSize, 0, 9999, 9999, txt);
    return brect[2] - brect[0];
}

void shadowText(int x, int y, char *txt) {
    if (doShadow) gdImageStringFT(renderImage, NULL, shadow, textFont, textFontSize, 0, x + doShadow, y + doShadow, txt);
    gdImageStringFT(renderImage, NULL, text, textFont, textFontSize, 0, x, y, txt);
}

int digitWidth(char *txt) {
    int brect[8];
    gdImageStringFT(renderImage, brect, text, digitFont, digitFontSize, 0, 9999, 9999, txt);
    return brect[2] - brect[0];
}

void shadowTextRight(int x, int y, char *txt) {
    int w = stringWidth(txt);
    shadowText(x - w, y, txt);
}

void shadowTextCenter(int x, int y, char *txt) {
    int w = stringWidth(txt);
    shadowText(x - (w/2), y, txt);
}


void shadowDigit(int x, int y, char *txt) {
    int p = x;
    int i;

    for (i = 0; i < strlen(txt); i++) {
        char d[2] = {txt[i], 0};
        int w = digitWidth(d);
        if (d[0] == '.') {
            p += digitDotSpace;
        } else {
            p += digitSpace;
        }
        if (doShadow) gdImageStringFT(renderImage, NULL, shadow, digitFont, digitFontSize, 0, p + doShadow - w, y + doShadow, d);
    }

    p = x;
    for (i = 0; i < strlen(txt); i++) {
        char d[2] = {txt[i], 0};

        int w = digitWidth(d);
        if (d[0] == '.') {
            p += digitDotSpace;
        } else {
            p += digitSpace;
        }
        gdImageStringFT(renderImage, NULL, text, digitFont, digitFontSize, 0, p - w, y, d);
    }
}
void updateImage() {

    int x, y;

    gdImageFilledRectangle(renderImage, 0, 0, width, height, bg);

    if (state == CLOSED || state == CLOSING) {
        if ((data.flags & DEVICE_ACTIVE) == DEVICE_ACTIVE) {
            state = OPENING;
        }
    }

    if (state == OPEN || state == OPENING) {
        if ((data.flags & DEVICE_ACTIVE) == 0) {
            state = CLOSING;
        }
    }

    if (state == CLOSING) {
        displayPos -= 0.1;
        if (displayPos <= 0) {
            displayPos = 0;
            state = CLOSED;
        }
    }

    if (state == OPENING) {
        displayPos += 0.1;
        if (displayPos >= 3.141592653) {
            state = OPEN;
            displayPos = 3.141592653;
        }
    }
    if (state == CLOSED) return;

    if (title != NULL) {
        shadowTextCenter(width / 2, 40, title);
    }


    int32_t maa = 0;
    int32_t mva = 0;
    int i;

    int32_t minMv = data.mv[0];
    int32_t maxMv = data.mv[0];
    
    for (i = 0; i < 10; i++) {
        maa += data.ma[i];
        mva += data.mv[i];

        if (data.mv[i] < minMv) minMv = data.mv[i];
        if (data.mv[i] > maxMv) maxMv = data.mv[i];
    }

    double ma = maa / 10000.0;
    double mv = mva / 10000.0;

    int32_t ripple = maxMv - minMv;

    shadowTextRight(450, 220, "Amps");
    shadowTextRight(450, 440, "Volts");
    
    char out[100];

    sprintf(out, "Ripple: %dmV", ripple);
    shadowTextRight(450, 280, out);

    if (ma >= 0) {
        sprintf(out, " %6.3f", ma);
    } else {
        sprintf(out, "%7.3f", ma);
    }
    shadowDigit(0, 180, out);

    if (mv >= 0) {
        sprintf(out, " %6.3f", mv);
    } else {
        sprintf(out, "%7.3f", mv);
    }
    shadowDigit(0, 400, out);

    double lma = data.ma[0] / 100.0;
    double lmv = data.mv[0] / 1000.0;
    for (i = 1; i < 10; i++) {
        gdImageLine(renderImage, lineStart + (i - 1) * lineSize, 220 - lma, lineStart + i * lineSize, 220 - data.ma[i] / 100.0, lineColor);
        gdImageLine(renderImage, lineStart + (i - 1) * lineSize, 440 - lmv, lineStart + i * lineSize, 440 - data.mv[i] / 1000.0, lineColor);
        lma = data.ma[i] / 100.0;
        lmv = data.mv[i] / 1000.0;
    }
}

void loadFrame() {
    int y = 0;
    int x = 0;

    int p = 0;

    double s = cos(displayPos);
    int pos = (width * ((s + 1) / 2.0));

    int bg1 = gdImageColorAllocate(outputImage, 0, 0, 0);
    gdImageFilledRectangle(outputImage, 0, 0, width, height, bg1);
    gdImageCopy(outputImage, renderImage, pos, 0, 0, 0, width, height);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            uint32_t pix = gdImageTrueColorPixel(outputImage, x, y);
            buffer[p++] = (pix >> 16);
            buffer[p++] = (pix >> 8);
            buffer[p++] = (pix >> 0);
        }
    }
}

void render() {


    updateImage();
    loadFrame();
}

time_t micros() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return 1000000 * tv.tv_sec + tv.tv_usec;
}

int main(int argc, char **argv) {

    time_t ts = 0;

    struct sockaddr_in sa;
    int sockfd;
    struct ip_mreq mreq;

    struct v4l2_capability vid_caps;
    struct v4l2_format vid_format;

    int opt;

    char *video_device = "/dev/video0";
    int fdwr = 0;

    while ((opt = getopt(argc, argv, "i:d:t:f:s:m:")) != -1) {
        switch (opt) {
            case 'd':
                video_device = strdup(optarg);
                break;
            case 'i':
                deviceId = strtoul(optarg, NULL, 16);
                break;
            case 't':
                title = optarg;
                break;
            case 'f':
                textFont = optarg;
                break;
            case 's':
                doShadow = atoi(optarg);
                break;
            case 'm':
                mcastIp = optarg;
                break;
            default:
                printf("Usage: %s [-d device]\n",
                    argv[0]);
                exit(10);
        }
    }

    printf("Connecting to %s...\n", video_device);

    fdwr = open(video_device, O_RDWR);
    if (fdwr < 0) {
        printf("Unable to open %s\n", video_device);
        exit(10);
    }

    int ret_code = 0;
    ret_code = ioctl(fdwr, VIDIOC_QUERYCAP, &vid_caps);

    if (ret_code < 0) {
        printf("Error querying caps: %s\n", strerror(errno));
        exit(10);
    }

    memset(&vid_format, 0, sizeof(vid_format));

    vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    vid_format.fmt.pix.width = width;
    vid_format.fmt.pix.height = height;
    vid_format.fmt.pix.sizeimage = width * height * 3;
    vid_format.fmt.pix.pixelformat = v4l2_fourcc('R','G','B','3');
    vid_format.fmt.pix.field = V4L2_FIELD_NONE;
    vid_format.fmt.pix.bytesperline = width * 3;
    vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

    ret_code = ioctl(fdwr, VIDIOC_S_FMT, &vid_format);


    printf("Buffer: %d\n", vid_format.fmt.pix.sizeimage);

    buffer = malloc(vid_format.fmt.pix.sizeimage);

    if (!buffer) {
        printf("Unable to allocate %d bytes.\n", vid_format.fmt.pix.sizeimage);
        exit(10);
    }

    renderImage = gdImageCreateTrueColor(width, height);
    outputImage = gdImageCreateTrueColor(width, height);

    bg = gdImageColorAllocate(renderImage, 0, 0, 0);
    shadow = gdImageColorAllocate(renderImage, 100, 0, 0);
    text = gdImageColorAllocate(renderImage, 255, 0,0);
    lineColor = gdImageColorAllocate(renderImage, 0, 200, 0);

    bzero(&sa, sizeof(sa));

    sa.sin_family=AF_INET;
    sa.sin_port=htons(port);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Unable to create socket\n");
        return 10;
    }

    int one = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        fprintf(stderr, "Unable to set SO_REUSEADDR\n");
        return 10;
    }


    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        fprintf(stderr, "Unable to bind socket\n");
        return 10;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(MCAST_IP);         
    mreq.imr_interface.s_addr = inet_addr(mcastIp);
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
        &mreq, sizeof(mreq)) < 0) {
        fprintf(stderr, "Error enabling multicast\n");
        return 10;
    }         


    while(1) {
        service(sockfd);

        if (micros() - ts >= 33300) {
            ts = micros();
            render();
            write(fdwr, buffer, vid_format.fmt.pix.sizeimage);
        }
    }


    return 0;
}
