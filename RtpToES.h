/*
*author:achilles_xu
*date:2015-03-21
*description:   use inPutRTPData function to input RTP data packet,
*               and get ES data(video and audio) through the callback function
*Test: tesing...........
*
*
****/

#ifndef RTP__TO__ES_H
#define RTP__TO__ES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <set>

//为了使用函数      ntohs()
#include <winsock2.h> 


//是否应该在别的地方放置这个
using namespace std;

#define MAX_PS_LENGTH  (0x100000)
#define MAX_PES_LENGTH (0xFFFF)
#define MAX_ES_LENGTH  (0x100000)

typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

enum MediaType
{
	AUDIO_T=0,
	VEDIO_T,
	UNKNOW_T
};

enum PSStatus
{
	ps_padding, //未知状态
	ps_ps,      //ps状态
	ps_sh,
	ps_psm,
	ps_pes,
	ps_pes_video,
	ps_pes_audio,
	ps_pes_last,
	ps_pes_lost, //pes包丢失数据（udp传输的rtp丢包）
	ps_pes_jump   //需要跳过的pes packet
};

//针对当前的摄像机（IP 192.168.9.123），其他的类型还未测试
enum NalType
{
	sei_Nal = 6,
	sps_Nal = 7,
	pps_Nal = 8,
	other,
	unknow
};


#pragma pack(1)
//rtp header
typedef struct rtpHeader{
#ifdef ORTP_BIGENDIAN
	uint16_t version : 2;
	uint16_t padbit : 1;
	uint16_t extbit : 1;
	uint16_t cc : 4;
	uint16_t markbit : 1;
	uint16_t paytype : 7;
#else
	uint16_t cc : 4;
	uint16_t extbit : 1;
	uint16_t padbit : 1;
	uint16_t version : 2;
	uint16_t paytype : 7;  //负载类型
	uint16_t markbit : 1;  //1表示前面的包为一个解码单元,0表示当前解码单元未结束
#endif
	uint16_t seq_number;  //序号
	uint32_t timestamp; //时间戳
	uint32_t ssrc;  //循环校验码
	//uint32_t csrc[16];

}RTP_header_t;
//ps header
typedef struct ps_header{
	unsigned char pack_start_code[4];  //'0x000001BA'

	unsigned char system_clock_reference_base21 : 2;
	unsigned char marker_bit : 1;
	unsigned char system_clock_reference_base1 : 3;
	unsigned char fix_bit : 2;    //'01'

	unsigned char system_clock_reference_base22;

	unsigned char system_clock_reference_base31 : 2;
	unsigned char marker_bit1 : 1;
	unsigned char system_clock_reference_base23 : 5;

	unsigned char system_clock_reference_base32;
	unsigned char system_clock_reference_extension1 : 2;
	unsigned char marker_bit2 : 1;
	unsigned char system_clock_reference_base33 : 5; //system_clock_reference_base 33bit

	unsigned char marker_bit3 : 1;
	unsigned char system_clock_reference_extension2 : 7; //system_clock_reference_extension 9bit

	unsigned char program_mux_rate1;

	unsigned char program_mux_rate2;
	unsigned char marker_bit5 : 1;
	unsigned char marker_bit4 : 1;
	unsigned char program_mux_rate3 : 6;

	unsigned char pack_stuffing_length : 3;
	unsigned char reserved : 5;
}ps_header_t;  //14
//system header
typedef struct sh_header
{
	unsigned char system_header_start_code[4]; //32

	unsigned char header_length[2];            //16 uimsbf

	uint32_t marker_bit1 : 1;   //1  bslbf
	uint32_t rate_bound : 22;   //22 uimsbf
	uint32_t marker_bit2 : 1;   //1 bslbf
	uint32_t audio_bound : 6;   //6 uimsbf
	uint32_t fixed_flag : 1;    //1 bslbf
	uint32_t CSPS_flag : 1;     //1 bslbf

	uint16_t system_audio_lock_flag : 1;  // bslbf
	uint16_t system_video_lock_flag : 1;  // bslbf
	uint16_t marker_bit3 : 1;             // bslbf
	uint16_t video_bound : 5;             // uimsbf
	uint16_t packet_rate_restriction_flag : 1; //bslbf
	uint16_t reserved_bits : 7;                //bslbf
	unsigned char reserved[6];
}sh_header_t; //18
//program stream map header
typedef struct psm_header{
	unsigned char promgram_stream_map_start_code[4];

	unsigned char program_stream_map_length[2];

	unsigned char program_stream_map_version : 5;
	unsigned char reserved1 : 2;
	unsigned char current_next_indicator : 1;

	unsigned char marker_bit : 1;
	unsigned char reserved2 : 7;

	unsigned char program_stream_info_length[2];
	unsigned char elementary_stream_map_length[2];
	unsigned char stream_type;
	unsigned char elementary_stream_id;
	unsigned char elementary_stream_info_length[2];
	unsigned char CRC_32[4];
	unsigned char reserved[16];
}psm_header_t; //36
//??
typedef struct pes_header
{
	unsigned char pes_start_code_prefix[3];
	unsigned char stream_id;
	unsigned short PES_packet_length;
}pes_header_t; //6
//??
typedef struct optional_pes_header{
	unsigned char original_or_copy : 1;
	unsigned char copyright : 1;
	unsigned char data_alignment_indicator : 1;
	unsigned char PES_priority : 1;
	unsigned char PES_scrambling_control : 2;
	unsigned char fix_bit : 2;

	unsigned char PES_extension_flag : 1;
	unsigned char PES_CRC_flag : 1;
	unsigned char additional_copy_info_flag : 1;
	unsigned char DSM_trick_mode_flag : 1;
	unsigned char ES_rate_flag : 1;
	unsigned char ESCR_flag : 1;
	unsigned char PTS_DTS_flags : 2;

	unsigned char PES_header_data_length;
}optional_pes_header_t;

typedef struct pes_status_data{
	bool pesTrued;
	PSStatus pesType;
	pes_header_t *pesPtr;
}pes_status_data_t;

typedef struct ps_rtp_data
{
	uint16_t seq_number;//用于set元素的排序
	uint16_t flow_sign;
	uint16_t ps_completed;//用于显示ps包是否完整
	bool ps_headered;
	uint32_t rtp_len;//不包含12字节的rtp头长度
	char rtp_data_ptr[0];//不包含12字节的rtp头数据

}ps_rtp_data_t;

typedef struct rtp_seq_rec{
	bool used;
	uint16_t seq_number;
}rtp_seq_rec_t;

typedef struct esFrameData{
	MediaType mediaType;
	uint64_t dts;
	uint64_t pts;
	uint32_t frame_len;
	char *frame_data;

}esFrameData_t;

#pragma pack()



//set的比较函数，此处采用函数对象
struct com_Seq{
	bool operator()(const ps_rtp_data_t *a1, const ps_rtp_data_t *a2) const
	{
		if (a1->flow_sign == a2->flow_sign)
		{
			return a1->seq_number < a2->seq_number;
		}
		else
		{
			uint32_t a1uInt = 0;
			uint32_t a2uInt = 0;
			if (1==a1->flow_sign)
			{
				if (a1->seq_number>1000)
					a1uInt = (uint32_t)a1->seq_number;
				else
					a1uInt = (uint32_t)65536 + (uint32_t)a1->seq_number;
			}
			else
			{
				a1uInt = (uint32_t)a1->seq_number;
			}

			if (1 == a2->flow_sign)
			{
				if (a2->seq_number>1000)
					a2uInt = (uint32_t)a2->seq_number;
				else
					a2uInt = (uint32_t)65536 + (uint32_t)a2->seq_number;
			}
			else
			{
				a2uInt = (uint32_t)a2->seq_number;
			}


			return a1uInt < a2uInt;
		}
	
	}
};


#ifndef AV_RB16
#   define AV_RB16(x)                           \
	((((const unsigned char*)(x))[0] << 8) | \
	((const unsigned char*)(x))[1])
#endif


static inline unsigned __int64 ff_parse_pes_pts(const unsigned char* buf) {
	return (unsigned __int64)(*buf & 0x0e) << 29 |
		(AV_RB16(buf + 1) >> 1) << 15 |
		AV_RB16(buf + 3) >> 1;
}

static unsigned __int64 get_pts(optional_pes_header* option)
{
	if (option->PTS_DTS_flags != 2 && option->PTS_DTS_flags != 3 && option->PTS_DTS_flags != 0)
	{
		return 0;
	}
	if ((option->PTS_DTS_flags & 2) == 2)
	{
		unsigned char* pts = (unsigned char*)option + sizeof(optional_pes_header);
		return ff_parse_pes_pts(pts);
	}
	return 0;
}

static unsigned __int64 get_dts(optional_pes_header* option)
{
	if (option->PTS_DTS_flags != 2 && option->PTS_DTS_flags != 3 && option->PTS_DTS_flags != 0)
	{
		return 0;
	}
	if ((option->PTS_DTS_flags & 3) == 3)
	{
		unsigned char* dts = (unsigned char*)option + sizeof(optional_pes_header)+5;
		return ff_parse_pes_pts(dts);
	}
	return 0;
}

bool inline is_ps_header(ps_header_t* ps)
{
	if (ps->pack_start_code[0] == 0 && ps->pack_start_code[1] == 0 && ps->pack_start_code[2] == 1 && ps->pack_start_code[3] == 0xBA)
		return true;
	return false;
}
bool inline is_sh_header(sh_header_t* sh)
{
	if (sh->system_header_start_code[0] == 0 && sh->system_header_start_code[1] == 0 && sh->system_header_start_code[2] == 1 && sh->system_header_start_code[3] == 0xBB)
		return true;
	return false;
}
bool inline is_psm_header(psm_header_t* psm)
{
	if (psm->promgram_stream_map_start_code[0] == 0 && psm->promgram_stream_map_start_code[1] == 0 && psm->promgram_stream_map_start_code[2] == 1 && psm->promgram_stream_map_start_code[3] == 0xBC)
		return true;
	return false;
}
bool inline is_pes_video_header(pes_header_t* pes)
{
	if (pes->pes_start_code_prefix[0] == 0 && pes->pes_start_code_prefix[1] == 0 && pes->pes_start_code_prefix[2] == 1 && pes->stream_id == 0xE0)
		return true;
	return false;
}
bool inline is_pes_audio_header(pes_header_t* pes)
{
	if (pes->pes_start_code_prefix[0] == 0 && pes->pes_start_code_prefix[1] == 0 && pes->pes_start_code_prefix[2] == 1 && pes->stream_id == 0xC0)
		return true;
	return false;
}

bool inline is_pes_header(pes_header_t* pes)
{
	if (pes->pes_start_code_prefix[0] == 0 && pes->pes_start_code_prefix[1] == 0 && pes->pes_start_code_prefix[2] == 1)
	{
		if (pes->stream_id == 0xC0 || pes->stream_id == 0xE0)
		{
			return true;
		}
	}
	return false;
}
PSStatus inline pes_type(pes_header_t* pes)
{
	if (pes->pes_start_code_prefix[0] == 0 && pes->pes_start_code_prefix[1] == 0 && pes->pes_start_code_prefix[2] == 1)
	{
		if (pes->stream_id == 0xC0)
		{
			return ps_pes_audio;
		}
		else if (pes->stream_id == 0xE0)
		{
			return ps_pes_video;
		}
		else if ((pes->stream_id == 0xBC) || (pes->stream_id == 0xBD) || (pes->stream_id == 0xBE) || (pes->stream_id == 0xBF))
		{
			return ps_pes_jump;
		}
		else if ((pes->stream_id == 0xF0) || (pes->stream_id == 0xF1) || (pes->stream_id == 0xF2) || (pes->stream_id == 0xF8))
		{
			return ps_pes_jump;
		}
	}
	return ps_padding;
}

//nal单元开头三个字节00 00 01 （Nal单元开始）
//nal单元开头四个字节00 00 00 01 （Nal单元开始）
typedef struct nal_unit_header3
{
	unsigned char start_code[3];
	unsigned char nal_type : 5;
	unsigned char nal_ref_idc : 2;
	unsigned char for_bit : 1;
	
}nal_unit_header3_t;

typedef struct nal_unit_header4
{
	unsigned char start_code[4];
	unsigned char nal_type : 5;
	unsigned char nal_ref_idc : 2;
	unsigned char for_bit : 1;
}nal_unit_header4_t;

NalType inline is_264_param(char *nal_str)
{
	nal_unit_header3_t *nalUnit3 = (nal_unit_header3_t *)nal_str;
	nal_unit_header4_t *nalUnit4 = (nal_unit_header4_t *)nal_str;

	if (nalUnit3->start_code[0] == 0 && nalUnit3->start_code[1] == 0 && nalUnit3->start_code[2] == 1)
	{
		if (sps_Nal == nalUnit3->nal_type)
		{
			return sps_Nal;
		}
		else if (pps_Nal == nalUnit3->nal_type)
		{
			return pps_Nal;
		}
		else if (sei_Nal == nalUnit3->nal_type)
		{
			return sei_Nal;
		}
		else
		{
			return other;
		}

	}
	else if (nalUnit4->start_code[0] == 0 && nalUnit4->start_code[1] == 0 && nalUnit4->start_code[2] == 0 && nalUnit4->start_code[3] == 1)
	{
		if (sps_Nal == nalUnit4->nal_type)
		{
			return sps_Nal;
		}
		else if (pps_Nal == nalUnit4->nal_type)
		{
			return pps_Nal;
		}
		else if (sei_Nal == nalUnit4->nal_type)
		{
			return sei_Nal;
		}
		else
		{
			return other;
		}
	}


	return unknow;
}


/*
* 通过回调函数将ES数据返回
*
*
*
*
*******/

typedef void(*getESdataCB)(esFrameData_t *fdata);


class CRTPtoES{
public:
	CRTPtoES(getESdataCB getEs);
	~CRTPtoES();

	//实现rtp包的排序，根据rtp sequence number


//音视频混合的PS流
/*****************************************************************************/
	//rtp包，包含ps相关头信息，
	//如果视频帧I帧，rtp_header--ps_header--system_header--ps_map_header--pes_packet(sps,pps,i frame,(extra data 可能包含))
	//P/B帧rtp_header--ps_header--pes_packet(sps,pps,i frame,(extra data 可能包含))

	//rtp包，不包含ps相关头信息，
	//rtp_header--video data(Elemetary stream)
	//rtp_header--pes_packet(audio frame)  
/*****************************************************************************/


//纯视频混合的PS流
/*****************************************************************************/
	//rtp包，包含ps相关头信息，
	//如果视频帧I帧，rtp_header--ps_header--system_header--ps_map_header--pes_packet(sps,pps,i frame,(extra data 可能包含))
	//P/B帧rtp_header--ps_header--pes_packet(sps,pps,i frame,(extra data 可能包含))

	//rtp包，不包含ps相关头信息，
	//rtp_header--video data(Elemetary stream) 
/*****************************************************************************/


/*
//纯音频的还未测试





*/
//纯音频混合的PS流
	/*****************************************************************************/
	//rtp包，包含ps相关头信息，
	//如果视频帧I帧，rtp_header--ps_header--system_header--ps_map_header--pes_packet(sps,pps,i frame,(extra data 可能包含))
	//P/B帧rtp_header--ps_header--pes_packet(sps,pps,i frame,(extra data 可能包含))

	//rtp包，不包含ps相关头信息，
	//rtp_header--video data(Elemetary stream)
	//rtp_header--pes_packet(audio frame)  
	/*****************************************************************************/


	bool inPutRTPData(char* pBuffer, unsigned int sz);
	//输出set中最后一个完整的PS帧，可以不调用此函数，丢弃最后一帧
	bool getFinalPSFrame();

private:
	//输出PS完整的一帧
	bool getPSFrame();

	//解析PS帧，并输出Element stream 
	pes_status_data_t pes_payload();

	bool naked_payload();

private:
	getESdataCB   m_getEsData;//回调函数

	//用于处理RTP的丢包和错序
	set<ps_rtp_data_t *, com_Seq> m_sDataSet;
	int m_nPsIndicator;//DUANG，当指示器的值达到3时，可以将完整的ps帧取出来，然后将指示器的值减一
	rtp_seq_rec_t m_arrRtpSeq[3];//记录rtp包的sequence号，此rtp包包含的ps包的包头；
	bool m_bAshBeforeFirst;       //只执行一次 第一个有效的00 00 01 BA前的无用的rtp包；

	bool m_bFirstIFrame;                        //找到第一个I帧开始输出


	//用于PS帧的解析变量
	PSStatus      m_status;                     //当前状态
	char          m_pPSBuffer[MAX_PS_LENGTH];   //PS缓冲区
	unsigned int  m_nPSWrtiePos;                //PS写入位置
	unsigned int  m_nPESIndicator;              //PES指针

	char          m_pPESBuffer[MAX_PES_LENGTH]; //PES缓冲区
	unsigned int  m_nPESLength;                 //PES数据长度

	ps_header_t*  m_ps;                         //PS头
	sh_header_t*  m_sh;                         //系统头
	psm_header_t* m_psm;                        //节目流头
	pes_header_t* m_pes;                        //PES头

	char         m_pESBuffer[MAX_ES_LENGTH];    //裸码流
	unsigned int m_nESLength;                   //裸码流长度


};


#endif
