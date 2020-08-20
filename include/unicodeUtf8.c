#include "unicodeUtf8.h"
#include "common.h"


uint32_t getByteNumOfEncodeUtf8(int value) {
  ASSERT(value > 0, "Can`t encode negative value!");

  if (value <= 0x7f) {
     return 1;
  }

  //此范围内数值编码为utf8需要2字节
  if (value <= 0x7ff) {
     return 2;
  }

  //此范围内数值编码为utf8需要3字节
  if (value <= 0xffff) {
     return 3;
  }

  //此范围内数值编码为utf8需要4字节
  if (value <= 0x10ffff) {
     return 4;
  }

  return 0;  //超过范围返回0
}

uint8_t encodeUtf8(uint8_t* buf, int value) {
  ASSERT(value > 0, "Can`t encode negative value!");

  if (value <= 0x7f) {   // 单个ascii字符需要1字节
     *buf = value & 0x7f;
     return 1;
  } else if (value <= 0x7ff) {  //此范围内数值编码为utf8需要2字节
     //先写入高字节
     *buf++ = 0xc0 | ((value & 0x7c0) >> 6);
     // 再写入低字节
     *buf = 0x80 | (value & 0x3f);
     return 2;
  } else if (value <= 0xffff) { //此范围内数值编码为utf8需要3字节
     // 先写入高字节
     *buf++ = 0xe0 | ((value & 0xf000) >> 12);
     //再写入中间字节
     *buf++ = 0x80 | ((value & 0xfc0) >> 6);
      //最后写入低字节
     *buf = 0x80 | (value & 0x3f);
     return 3;
  } else if (value <= 0x10ffff) { //此范围内数值编码为utf8需要4字节
     *buf++ = 0xf0 | ((value & 0x1c0000) >> 18);
     *buf++ = 0x80 | ((value & 0x3f000) >> 12);
     *buf++ = 0x80 | ((value & 0xfc0) >> 6);
     *buf = 0x80 | (value & 0x3f);
     return 4;
  }

  NOT_REACHED();
  return 0;
}

uint32_t getByteNumOfDecodeUtf8(uint8_t byte) {

  if ((byte & 0xc0) == 0x80) {
     return 0;
  }

  if ((byte & 0xf8) == 0xf0) {
     return 4;
  }

  if ((byte & 0xf0) == 0xe0) {
     return 3;
  }

  if ((byte & 0xe0) == 0xc0) {
     return 2;
  }

  return 1;   //ascii码
}

int decodeUtf8(const uint8_t* bytePtr, uint32_t length) {
  if (*bytePtr <= 0x7f) {
     return *bytePtr;
  }

  int value;
  uint32_t remainingBytes;

  if ((*bytePtr & 0xe0) == 0xc0) {
     //若是2字节的utf8
     value = *bytePtr & 0x1f;
     remainingBytes = 1;
  } else if ((*bytePtr & 0xf0) == 0xe0) {
     //若是3字节的utf8
     value = *bytePtr & 0x0f;
     remainingBytes = 2;
  } else if ((*bytePtr & 0xf8) == 0xf0) {
     //若是4字节的utf8
     value = *bytePtr & 0x07;
     remainingBytes = 3;
  } else {   //非法编码
     return -1;
  }

  if (remainingBytes > length - 1) {
     return -1;
  }

  while (remainingBytes > 0) {
     bytePtr++;
     remainingBytes--;
     //高2位必须是10
     if ((*bytePtr & 0xc0) != 0x80) {
  return -1;
     }

     //从次高字节往低字节,不断累加各字节的低6位
     value = value << 6 | (*bytePtr & 0x3f);
  }
  return value;
}
