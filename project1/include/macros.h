enum statePacket {
    packet_START, 
    packet_FLAG1, 
    packet_A, 
    packet_STOP
};

enum setState{
    START_STATE,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC,
    STOP_STATE
};



#define FLAG 0x7E
#define A 0x03
#define A_RX 0x01
#define C_SET 0x03
#define C_UA 0x07
#define BCC_SET (A^C_SET)
#define BCC_UA (A^C_UA)
#define C_DISC 0x0B

#define ESC 0x7D
