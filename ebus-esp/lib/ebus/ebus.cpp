unsigned char crc8_calc(unsigned char data, unsigned char crc_init) {
   unsigned char crc;
   unsigned char polynom;
   int i;
 
   crc = crc_init;
   for (i = 0; i < 8; i++) {
      if (crc & 0x80) {
         polynom = (unsigned char) 0x9B;
      } else {
         polynom = (unsigned char) 0;
      }
      crc = (unsigned char)((crc & ~0x80) << 1);
      if (data & 0x80) {
         crc = (unsigned char)(crc | 1) ;
      }
      crc = (unsigned char)(crc ^ polynom);
      data = (unsigned char)(data << 1);
   }
   return (crc);
}

unsigned char crc8_array(unsigned char data[], unsigned int length) {
   int i;
   unsigned char uc_crc;
   uc_crc = (unsigned char) 0;
   for (i = 0; i < length; i++) {
      uc_crc = crc8_calc(data[i], uc_crc);
   }
   return (uc_crc);
}