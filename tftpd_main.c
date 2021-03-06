#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "stdint.h"
#include "string.h"
#include "errno.h"
#include "fcntl.h"
#include "sys/socket.h"
#include "netinet/in.h"

#define TFTPD_PORT        69
#define TFTP_HDR_LEN      4
#define TFTP_DATA_MAX_LEN 512
char tftpd_path[256] = {0};
char rcv_data_buff[TFTP_DATA_MAX_LEN] = {0};
char snd_data_buff[TFTP_DATA_MAX_LEN + TFTP_HDR_LEN] = {0};


#define PRINT_ERRNO() printf("%s:%d, errno=%s\n", __FILE__, __LINE__, strerror(errno));
#define PRINT_ERRNO_RET(err)  do{if(err) {PRINT_ERRNO(); return err;}}while(0)

#define ERR_SOCKET_CREATE    1
#define ERR_FILE_NOT_EXISTS  2

void show_help()
{
	printf("TFTP Usage: AlbertTftpd path\n");
}

void tftpd_parse_path(int argc, char*argv[])
{
	if(1 == argc)
	{
		getcwd(tftpd_path, sizeof(tftpd_path));
	}
	else
	{
		strncpy(tftpd_path, argv[1], sizeof(tftpd_path)-1);
	}
	
	printf("path: %s\n", tftpd_path);
}

/*
TFTP Formats(TFTP格式)

   Type   Op #     Format without header

          2 bytes    string   1 byte     string   1 byte

          -----------------------------------------------

   RRQ/  | 01/02 |  Filename  |   0  |    Mode    |   0  |

   WRQ    -----------------------------------------------

          2 bytes    2 bytes       n bytes

          ---------------------------------

   DATA  | 03    |   Block #  |    Data    |

          ---------------------------------

          2 bytes    2 bytes

          -------------------

   ACK   | 04    |   Block #  |

          --------------------

          2 bytes  2 bytes        string    1 byte

          ----------------------------------------

   ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |

*/
enum
{
	TFTP_RRQ = 01,
	TFTP_WRQ,
	TFTP_DATA,
	TFTP_ACK,
	TFTP_ERROR	
};

int tftpd_process(char* in_data, int in_len, char *out_data, int *out_len)
{
	uint16_t op = *(uint16_t *)in_data;
	op = ntohs(op);
	printf("op=%u\n", op);
	switch(op)
	{
		case TFTP_RRQ:
		{
			char file_name[100] = {0};
			int fd = 0;
			int len = 0;
			static int block = 0;

			strcpy(file_name,tftpd_path);
			strcat(file_name, "/");
			strcat(file_name, &in_data[2]);
			
			printf("file_name %s\n", file_name);
			fd = open(file_name, O_RDWR);
			if(fd < 0)
			{
				printf("fd=%d\n", fd);
				return ERR_FILE_NOT_EXISTS;
			}
			
			len = read(fd, out_data + TFTP_HDR_LEN, TFTP_DATA_MAX_LEN);
			if(len > 0)
			{
				printf("read len = %d\n", len);
				out_data[0] =0;
				out_data[1] =TFTP_DATA;				
				block++;
				out_data[2] =0;
				out_data[3] =block;	

				len += TFTP_HDR_LEN;
			}
			*out_len = len;

			close(fd);
			break;
		}
		
		case TFTP_WRQ:
			break;
		
		case TFTP_DATA:
			break;
		
		case TFTP_ACK:
			break;
		
		case TFTP_ERROR:
			break;
	}
}


int tftpd_socket()
{
	int fd;
	int err = 0;
	struct sockaddr_in addr;	
	socklen_t addr_len = sizeof(struct sockaddr_in);
	ssize_t len;
	ssize_t snd_len;
	
	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(fd < 0)
	{
		PRINT_ERRNO();
		return ERR_SOCKET_CREATE;
	}

	addr.sin_family = AF_INET;
	addr.sin_port   = htons(TFTPD_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	err = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	if(err)
	{
		PRINT_ERRNO();
		return ERR_SOCKET_CREATE;
	}

	/* server端，UDP不支持listen、accept。
	   client端, TCP: 需要connect服务器
	             UDP: 可以用connect，recv/send来收发包；也可以不用connect， recvfrom/sendto来收发包
	             client端一般不需要bind。如果想指定client的port，需要bind。

	   flag: 0会block
	 */
	memset(&addr, 0, sizeof(addr));
	while('q' != getchar())
	{
		len = recvfrom(fd, &rcv_data_buff, sizeof(rcv_data_buff), 0,
					(struct sockaddr*)&addr, &addr_len);

		if(len <= 0)
		{
			PRINT_ERRNO();
			break;
		}

		printf("rcv data:len=%u, %x%x%x%x\n", len, rcv_data_buff[0],rcv_data_buff[1],rcv_data_buff[2],rcv_data_buff[3]);
		
		tftpd_process(rcv_data_buff, len, snd_data_buff, &snd_len);
		
		printf("snd data:len=%u, %x%x%x%x\n", snd_len, snd_data_buff[0],snd_data_buff[1],snd_data_buff[2],snd_data_buff[3]);
		printf("port=%u, ip=%d\n", addr.sin_port, addr.sin_addr.s_addr);
		len = sendto(fd, snd_data_buff, snd_len, 0 , (struct sockaddr*)&addr, addr_len);
		if(len <= 0)
		{
			PRINT_ERRNO();
			break;
		}
	}

	close(fd);

	return 1;
}

int main(int argc, char *argv[])
{
	tftpd_parse_path(argc, argv);

	tftpd_socket();
	return 0;
}
