
#include "mbed.h"
#include "EasyAttach_CameraAndLCD.h"
#include "SdUsbConnect.h"
#include "JPEG_Converter.h"
#include "dcache-control.h"
#include "opencv2/opencv.hpp"
#include "tinypcl.hpp"

#define MOUNT_NAME             "storage"

/* Video input and LCD layer 0 output */
#define VIDEO_FORMAT           (DisplayBase::VIDEO_FORMAT_YCBCR422)
#define GRAPHICS_FORMAT        (DisplayBase::GRAPHICS_FORMAT_YCBCR422)
#define WR_RD_WRSWA            (DisplayBase::WR_RD_WRSWA_32_16BIT)
#define DATA_SIZE_PER_PIC      (2u)

/*! Frame buffer stride: Frame buffer stride should be set to a multiple of 32 or 128
    in accordance with the frame buffer burst transfer mode. */
#define VIDEO_PIXEL_HW         (320u)  /* VGA */
#define VIDEO_PIXEL_VW         (240u)  /* VGA */

#define FRAME_BUFFER_STRIDE    (((VIDEO_PIXEL_HW * DATA_SIZE_PER_PIC) + 31u) & ~31u)
#define FRAME_BUFFER_HEIGHT    (VIDEO_PIXEL_VW)

#if defined(__ICCARM__)
#pragma data_alignment=32
static uint8_t user_frame_buffer0[FRAME_BUFFER_STRIDE * FRAME_BUFFER_HEIGHT]@ ".mirrorram";
#pragma data_alignment=4
#else
static uint8_t user_frame_buffer0[FRAME_BUFFER_STRIDE * FRAME_BUFFER_HEIGHT]__attribute((section("NC_BSS"),aligned(32)));
#endif
static int file_name_index = 1;
/* jpeg convert */
static JPEG_Converter Jcu;
#if defined(__ICCARM__)
#pragma data_alignment=32
static uint8_t JpegBuffer[1024 * 63];
#else
static uint8_t JpegBuffer[1024 * 63]__attribute((aligned(32)));
#endif

DisplayBase Display;
DigitalIn   button0(USER_BUTTON);
DigitalOut  led1(LED1);
DigitalOut  led2(LED2);

static void save_image_bmp(void) {
    // Transform buffer into OpenCV Mat
    cv::Mat img_yuv(VIDEO_PIXEL_VW, VIDEO_PIXEL_HW, CV_8UC2, user_frame_buffer0);

    // Convert from YUV422 to grayscale
    cv::Mat img_gray;
    cv::cvtColor(img_yuv, img_gray, CV_YUV2GRAY_YUY2);

    char file_name[32];
    sprintf(file_name, "/"MOUNT_NAME"/img_%d.bmp", file_name_index++);
    cv::imwrite(file_name, img_gray);
    printf("Saved file %s\r\n", file_name);
}

static void save_image_jpg(void) {
    size_t jcu_encode_size;
    JPEG_Converter::bitmap_buff_info_t bitmap_buff_info;
    JPEG_Converter::encode_options_t   encode_options;

    bitmap_buff_info.width              = VIDEO_PIXEL_HW;
    bitmap_buff_info.height             = VIDEO_PIXEL_VW;
    bitmap_buff_info.format             = JPEG_Converter::WR_RD_YCbCr422;
    bitmap_buff_info.buffer_address     = (void *)user_frame_buffer0;

    encode_options.encode_buff_size     = sizeof(JpegBuffer);
    encode_options.p_EncodeCallBackFunc = NULL;
    encode_options.input_swapsetting    = JPEG_Converter::WR_RD_WRSWA_32_16_8BIT;

    jcu_encode_size = 0;
    dcache_invalid(JpegBuffer, sizeof(JpegBuffer));
    if (Jcu.encode(&bitmap_buff_info, JpegBuffer, &jcu_encode_size, &encode_options) != JPEG_Converter::JPEG_CONV_OK) {
        jcu_encode_size = 0;
    }

    char file_name[32];
    sprintf(file_name, "/"MOUNT_NAME"/img_%d.jpg", file_name_index++);
    FILE * fp = fopen(file_name, "w");
    fwrite(JpegBuffer, sizeof(char), (int)jcu_encode_size, fp);
    fclose(fp);
    printf("Saved file %s\r\n", file_name);
}

static void Start_Video_Camera(void) {
    // Video capture setting (progressive form fixed)
    Display.Video_Write_Setting(
        DisplayBase::VIDEO_INPUT_CHANNEL_0,
        DisplayBase::COL_SYS_NTSC_358,
        (void *)user_frame_buffer0,
        FRAME_BUFFER_STRIDE,
        VIDEO_FORMAT,
        WR_RD_WRSWA,
        VIDEO_PIXEL_VW,
        VIDEO_PIXEL_HW
    );
    EasyAttach_CameraStart(Display, DisplayBase::VIDEO_INPUT_CHANNEL_0);
}

// 点群データ
PointCloud point_cloud;

int main() {
    // Initialize the background to black
    for (uint32_t i = 0; i < sizeof(user_frame_buffer0); i += 2) {
        user_frame_buffer0[i + 0] = 0x10;
        user_frame_buffer0[i + 1] = 0x80;
    }

    // Camera
    EasyAttach_Init(Display);
    Start_Video_Camera();

    // SD & USB
    SdUsbConnect storage(MOUNT_NAME);

    while (1) {
        storage.wait_connect();
        if (button0 == 0) {
            led1 = 1;
            // save_image_bmp(); // save as bitmap
            // save_image_jpg(); // save as jpeg

            // Transform buffer into OpenCV Mat
            cv::Mat img_yuv(VIDEO_PIXEL_VW, VIDEO_PIXEL_HW, CV_8UC2, user_frame_buffer0);

            // グレースケール画像に変換
            cv::Mat img_tmp, img_gray;
            cv::cvtColor(img_yuv, img_tmp, CV_YUV2GRAY_YUY2);

            // 平滑化
            cv::medianBlur(img_tmp, img_gray, 3);

            // グレースケール画像の保存
            char file_name[32];
            sprintf(file_name, "/"MOUNT_NAME"/img_%d.bmp", file_name_index);
            cv::imwrite(file_name, img_gray);
            printf("Saved file %s\r\n", file_name);

            // 画像を1pixelごとに処理
            for (unsigned int y = 0; y < VIDEO_PIXEL_VW; y++) {
                for (unsigned int x = 0; x < VIDEO_PIXEL_HW; x++) {

                    // 画像の明度を深さに変換
                    unsigned int depth = point_cloud.SIZE_Z - (unsigned int)((double)img_gray.at<unsigned char>(y, x) / 255.0 * point_cloud.SIZE_Z);

                    // 深さに応じて3次元データを削る
                    for (unsigned int z=0; z<depth; z++) {
                        point_cloud.set(x, y, z, 0);
                    }

                }
            }

            // 底を作る
            for (unsigned int y = 0; y < VIDEO_PIXEL_VW; y++) {
                for (unsigned int x = 0; x < VIDEO_PIXEL_HW; x++) {
                        point_cloud.set(x, y, 0, 0);
                        point_cloud.set(x, y, 1, 1);
                }
            }

            printf("saving STL file\r\n");

            // STLファイルを保存
            sprintf(file_name, "/storage/result_%d.stl", file_name_index);
            point_cloud.save_as_stl(file_name);
            printf("Saved file %s\r\n", file_name);

            led1 = 0;

            file_name_index++;
            point_cloud.clear();
        }
    }
}

