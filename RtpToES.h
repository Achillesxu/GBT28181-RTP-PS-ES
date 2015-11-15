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

//Ϊ��ʹ�ú���      ntohs()
#include <winsock2.h> 


//�Ƿ�Ӧ���ڱ�ĵط��������
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
	ps_padding, //δ֪״̬
	ps_ps,      //ps״̬
	ps_sh,
	ps_psm,
	ps_pes,
	ps_pes_video,
	ps_pes_audio,
	ps_pes_last,
	ps_pes_lost, //pes����ʧ���ݣ�udp�����rtp������
	ps_pes_jump   //��Ҫ������pes packet
};

//��Ե�ǰ���������IP 192.168.9.123�������������ͻ�δ����
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
	uint16_t paytype : 7;  //��������
	uint16_t markbit : 1;  //1��ʾǰ��İ�Ϊһ�����뵥Ԫ,0��ʾ��ǰ���뵥Ԫδ����
#endif
	uint16_t seq_number;  //���
	uint32_t timestamp; //ʱ���
	uint32_t ssrc;  //ѭ��У����
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
	uint16_t seq_number;//����setԪ�ص�����
	uint16_t flow_sign;
	uint16_t ps_completed;//������ʾps���Ƿ�����
	bool ps_headered;
	uint32_t rtp_len;//������12�ֽڵ�rtpͷ����
	char rtp_data_ptr[0];//������12�ֽڵ�rtpͷ����

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



//set�ıȽϺ������˴����ú�������
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

//nal��Ԫ��ͷ�����ֽ�00 00 01 ��Nal��Ԫ��ʼ��
//nal��Ԫ��ͷ�ĸ��ֽ�00 00 00 01 ��Nal��Ԫ��ʼ��
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
* ͨ���ص�������ES���ݷ���
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

	//ʵ��rtp�������򣬸���rtp sequence number


//����Ƶ��ϵ�PS��
/*****************************************************************************/
	//rtp��������ps���ͷ��Ϣ��
	//�����Ƶ֡I֡��rtp_header--ps_header--system_header--ps_map_header--pes_packet(sps,pps,i frame,(extra data ���ܰ���))
	//P/B֡rtp_header--ps_header--pes_packet(sps,pps,i frame,(extra data ���ܰ���))

	//rtp����������ps���ͷ��Ϣ��
	//rtp_header--video data(Elemetary stream)
	//rtp_header--pes_packet(audio frame)  
/*****************************************************************************/


//����Ƶ��ϵ�PS��
/*****************************************************************************/
	//rtp��������ps���ͷ��Ϣ��
	//�����Ƶ֡I֡��rtp_header--ps_header--system_header--ps_map_header--pes_packet(sps,pps,i frame,(extra data ���ܰ���))
	//P/B֡rtp_header--ps_header--pes_packet(sps,pps,i frame,(extra data ���ܰ���))

	//rtp����������ps���ͷ��Ϣ��
	//rtp_header--video data(Elemetary stream) 
/*****************************************************************************/


/*
//����Ƶ�Ļ�δ����





*/
//����Ƶ��ϵ�PS��
	/*****************************************************************************/
	//rtp��������ps���ͷ��Ϣ��
	//�����Ƶ֡I֡��rtp_header--ps_header--system_header--ps_map_header--pes_packet(sps,pps,i frame,(extra data ���ܰ���))
	//P/B֡rtp_header--ps_header--pes_packet(sps,pps,i frame,(extra data ���ܰ���))

	//rtp����������ps���ͷ��Ϣ��
	//rtp_header--video data(Elemetary stream)
	//rtp_header--pes_packet(audio frame)  
	/*****************************************************************************/


	bool inPutRTPData(char* pBuffer, unsigned int sz);
	//���set�����һ��������PS֡�����Բ����ô˺������������һ֡
	bool getFinalPSFrame();

private:
	//���PS������һ֡
	bool getPSFrame();

	//����PS֡�������Element stream 
	pes_status_data_t pes_payload();

	bool naked_payload();

private:
	getESdataCB   m_getEsData;//�ص�����

	//���ڴ���RTP�Ķ����ʹ���
	set<ps_rtp_data_t *, com_Seq> m_sDataSet;
	int m_nPsIndicator;//DUANG����ָʾ����ֵ�ﵽ3ʱ�����Խ�������ps֡ȡ������Ȼ��ָʾ����ֵ��һ
	rtp_seq_rec_t m_arrRtpSeq[3];//��¼rtp����sequence�ţ���rtp��������ps���İ�ͷ��
	bool m_bAshBeforeFirst;       //ִֻ��һ�� ��һ����Ч��00 00 01 BAǰ�����õ�rtp����

	bool m_bFirstIFrame;                        //�ҵ���һ��I֡��ʼ���


	//����PS֡�Ľ�������
	PSStatus      m_status;                     //��ǰ״̬
	char          m_pPSBuffer[MAX_PS_LENGTH];   //PS������
	unsigned int  m_nPSWrtiePos;                //PSд��λ��
	unsigned int  m_nPESIndicator;              //PESָ��

	char          m_pPESBuffer[MAX_PES_LENGTH]; //PES������
	unsigned int  m_nPESLength;                 //PES���ݳ���

	ps_header_t*  m_ps;                         //PSͷ
	sh_header_t*  m_sh;                         //ϵͳͷ
	psm_header_t* m_psm;                        //��Ŀ��ͷ
	pes_header_t* m_pes;                        //PESͷ

	char         m_pESBuffer[MAX_ES_LENGTH];    //������
	unsigned int m_nESLength;                   //����������


};


#endif
