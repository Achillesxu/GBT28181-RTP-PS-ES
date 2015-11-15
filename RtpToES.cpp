/*
*
*
*
*Test: tesing...........
*
*
*
****/

#include "RtpToES.h"
/*
test the class if there is some leaking in memory
*/
#define LEAK_CHECK
#include "WinLeakCheck.h"

CRTPtoES::CRTPtoES(getESdataCB getEs) :m_getEsData(getEs)
{
	m_bAshBeforeFirst = false;
	
	m_bFirstIFrame = false;	
	m_nPsIndicator = 0;
	for (int i = 0; i < 3; i++)
	{
		m_arrRtpSeq[i].used = false;
		m_arrRtpSeq[i].seq_number = 0;
	}

	m_status = ps_padding;
	m_nESLength = m_nPESIndicator = m_nPSWrtiePos = m_nPESLength = 0;

}

CRTPtoES::~CRTPtoES()
{
	//释放资源
	if (!m_sDataSet.empty())
	{
		set<ps_rtp_data_t *, com_Seq>::iterator it;
		for (it = m_sDataSet.begin(); it != m_sDataSet.end(); it++)
		{
			free(*it);
		}
		m_sDataSet.clear();
	}

}


bool CRTPtoES::inPutRTPData(char* pBuffer, unsigned int sz)
{
	bool achieved = false;

	char* data = (pBuffer + sizeof(RTP_header_t));
	unsigned int length = sz - sizeof(RTP_header_t);

	ps_rtp_data_t *tempPsRtpData = (ps_rtp_data_t *)malloc(sizeof(ps_rtp_data_t)+length + 1);
	::memset(tempPsRtpData, 0, sizeof(ps_rtp_data_t)+length + 1);
	if (!tempPsRtpData)
	{
		fprintf(stderr, "ps_rtp_data_t allocating failed\n");
		return achieved;
	}

	RTP_header_t *tempRTPHrd = (RTP_header_t *)pBuffer;
	//获取rtp的sequence number
	tempPsRtpData->seq_number = ntohs(tempRTPHrd->seq_number);
	tempPsRtpData->ps_completed = tempRTPHrd->markbit;

	if (tempPsRtpData->seq_number >= 65000)
	{
		tempPsRtpData->flow_sign = 0;
	}
	else
	{
		tempPsRtpData->flow_sign = 1;
	}

	printf("input rtp sequence number is %d\n", tempPsRtpData->seq_number);
	//检查这个rtp包中，是否包含ps的头，就是检查是否有 00 00 01 BA
	//两个临时变量，
	ps_header_t *tempPs = NULL;
	unsigned int  mPackIndicator = 0;
	//线性搜索，可能比较耗费时间,
	//暂时只搜索前三分之一的位置
	for (; mPackIndicator<length / 3; mPackIndicator++)
	{

		tempPs = (ps_header_t*)(data + mPackIndicator);

		if (is_ps_header(tempPs))
		{
			//如果找到标记位
			tempPsRtpData->ps_headered = true;
			//标记
			m_nPsIndicator++;
			for (int i = 0; i < 3; i++)
			{
				if (m_arrRtpSeq[i].used == false)
				{
					m_arrRtpSeq[i].used = true;
					m_arrRtpSeq[i].seq_number = tempPsRtpData->seq_number;
					break;
				}
			}
			break;
		}
	}

	tempPsRtpData->rtp_len = length;
	memcpy(tempPsRtpData->rtp_data_ptr, data, length);

	
	//ps_rtp_data_t结构填充完毕
	m_sDataSet.insert(tempPsRtpData);

	data = NULL;
	tempPsRtpData = NULL;
	tempRTPHrd = NULL;
	tempPs = NULL;
	/*
	::printf("\n\n");
	for (set<ps_rtp_data_t *, com_Seq>::iterator it = m_sDataSet.begin(); it != m_sDataSet.end();it++)
	{
	::printf("the rtp sequence is %d\n",(*it)->seq_number);
	}*/
	
	if (3 == m_nPsIndicator)//达到要求set里面有三个包，包含00 00 01 BA,可以输出一个PS帧
	{
		::printf("ready to output a complete PS frame\n");
		if (!m_bAshBeforeFirst)
		{
			//删除第一个00 00 01 BA标志包前的数据包
			for (set<ps_rtp_data_t *, com_Seq>::iterator it = m_sDataSet.begin(); it != m_sDataSet.end();)
			{
				printf("++++++++++++++++++++++++the sequence is %d\n", (*it)->seq_number);
				printf("________________________the sequence is %d\n", m_arrRtpSeq[0].seq_number);
				if ((*it)->seq_number < m_arrRtpSeq[0].seq_number)
				{
					free(*it);
					m_sDataSet.erase(it++);
				}
				else
				{
					break;
				}
			}
			m_bAshBeforeFirst = true;
		}

		achieved = getPSFrame();
		return achieved;
	}
	else
	{
		
		achieved = true;
		return achieved;
	}
	

}


bool CRTPtoES::getPSFrame()
{
	bool achieved = false;
	bool discarded = false;

	//printf("______________________output the ps buffer beginning sequence is %d\n", m_arrRtpSeq[0].seq_number);

	//检测PS帧内是否有丢包
	
	if (false == (*m_sDataSet.begin())->ps_headered)
	{
		fprintf(stderr, "the packet including ps_header lost! sequence is %d   \n", (*m_sDataSet.begin())->seq_number);
		return achieved;
	}

	uint32_t tPSFrmLen = 0;//临时记录m_nPSFrmLen长度
	uint16_t tPSCompleted = 0;//临时记录，整个ps包是否完整，不完整的话需要丢弃
	m_nPSWrtiePos = 0;


	memcpy((m_pPSBuffer + tPSFrmLen), ((*m_sDataSet.begin())->rtp_data_ptr), (*m_sDataSet.begin())->rtp_len);
	tPSFrmLen += (*m_sDataSet.begin())->rtp_len;
	//记录第一个rtp包的sequence number
	uint16_t tempLastSeq = (*m_sDataSet.begin())->seq_number;
	//tPSCompleted += (*m_sDataSet.begin())->ps_completed;

	set<ps_rtp_data_t *, com_Seq>::iterator itFirst = m_sDataSet.begin();
	free(*itFirst);
	m_sDataSet.erase(itFirst++);//将迭代器移动到下一个rtp包的位置

	
	for (set<ps_rtp_data_t *, com_Seq>::iterator it = m_sDataSet.begin(); it != m_sDataSet.end();)
	{
		if ((*it)->seq_number == m_arrRtpSeq[1].seq_number)
		{
			if ((tempLastSeq + 1) == m_arrRtpSeq[1].seq_number)
			{
				tPSCompleted = 1;
			}
			//如果ps帧，只包含一个rtp包
			break;
		}
		//rtp包已经移除头部12字节

		if (((tempLastSeq + 1) == (*it)->seq_number))
		{
			memcpy((m_pPSBuffer + tPSFrmLen), ((*it)->rtp_data_ptr), (*it)->rtp_len);
			tPSFrmLen += (*it)->rtp_len;
			//tPSCompleted += (*it)->ps_completed;
			tempLastSeq = (*it)->seq_number;
			//移除已经复制的数据
			free(*it);
			m_sDataSet.erase(it++);

		}
		else
		{
			discarded = true;
			free(*it);
			m_sDataSet.erase(it++);
		}

	}

	//设置状态标记
	m_nPsIndicator--;
	m_arrRtpSeq[0].seq_number = m_arrRtpSeq[1].seq_number;
	m_arrRtpSeq[1].seq_number = m_arrRtpSeq[2].seq_number;

	m_arrRtpSeq[2].seq_number = 0;
	m_arrRtpSeq[2].used = false;
	
	if ((false == discarded) && (1==tPSCompleted))
	{
		m_nPSWrtiePos = tPSFrmLen;
		::printf("PS frame length  is: %d\n", m_nPSWrtiePos);
		//进行下一步的ps帧的解析工作
		//调用naked_payload
		achieved = naked_payload();
		return achieved;

	}

	achieved = true;

	return achieved;
}

	
bool CRTPtoES::getFinalPSFrame()
{
	bool achieved = false;

	achieved = getPSFrame();

	return achieved;

}

pes_status_data_t CRTPtoES::pes_payload()
{
	//作为完整的ps包，开头字节肯定是00 00 01 BA
	if (m_status == ps_padding)
	{
		for (; m_nPESIndicator<m_nPSWrtiePos; m_nPESIndicator++)
		{
			m_ps = (ps_header_t*)(m_pPSBuffer + m_nPESIndicator);
			if (is_ps_header(m_ps))
			{
				m_status = ps_ps;
				break;
			}
		}
	}


	if (m_status == ps_ps)
	{
		for (; m_nPESIndicator<m_nPSWrtiePos; m_nPESIndicator++)
		{
			m_sh = (sh_header_t*)(m_pPSBuffer + m_nPESIndicator);
			m_pes = (pes_header_t*)(m_pPSBuffer + m_nPESIndicator);
			if (is_sh_header(m_sh))
			{
				m_status = ps_sh;
				break;
			}
			else if (is_pes_header(m_pes))
			{
				m_status = ps_pes;
				break;
			}
		}
	}
	if (m_status == ps_sh)
	{
		for (; m_nPESIndicator<m_nPSWrtiePos; m_nPESIndicator++)
		{
			m_psm = (psm_header_t*)(m_pPSBuffer + m_nPESIndicator);
			m_pes = (pes_header_t*)(m_pPSBuffer + m_nPESIndicator);
			if (is_psm_header(m_psm))
			{
				m_status = ps_psm;//冲掉s_sh状态
				break;
			}
			if (is_pes_header(m_pes))
			{
				m_status = ps_pes;
				break;
			}
		}
	}
	if (m_status == ps_psm)
	{
		for (; m_nPESIndicator<m_nPSWrtiePos; m_nPESIndicator++)
		{
			m_pes = (pes_header_t*)(m_pPSBuffer + m_nPESIndicator);
			if (is_pes_header(m_pes))
			{
				m_status = ps_pes;
				break;
			}
		}
	}
	if (m_status == ps_pes)
	{
		//寻找下一个pes

		unsigned short PES_packet_length = ntohs(m_pes->PES_packet_length);
		//下面还有pes需要解析
		if ((m_nPESIndicator + PES_packet_length + sizeof(pes_header_t)) < m_nPSWrtiePos)
		{
			char* next = (m_pPSBuffer + m_nPESIndicator + sizeof(pes_header_t)+PES_packet_length);
			pes_header_t* pes = (pes_header_t*)next;

			m_nPESLength = PES_packet_length + sizeof(pes_header_t);
			memcpy(m_pPESBuffer, m_pes, m_nPESLength);
			PSStatus tempPSSt = pes_type(pes);
			if ((ps_pes_video == tempPSSt) || (ps_pes_audio == tempPSSt) || (ps_pes_jump == tempPSSt))
			{

				int remain = m_nPSWrtiePos - (next - m_pPSBuffer);
				memcpy(m_pPSBuffer, next, remain);
				m_nPSWrtiePos = remain; m_nPESIndicator = 0;
				m_pes = (pes_header_t*)m_pPSBuffer;

				pes_status_data_t pesStatusTemp;

				pesStatusTemp.pesTrued = true;
				pesStatusTemp.pesPtr = (pes_header_t*)m_pPESBuffer;
				
				if (ps_pes_jump == tempPSSt)
				{				
					pesStatusTemp.pesType = ps_pes_jump;		
				}
				/*
				else
				{
					pesStatusTemp.pesType = ps_pes_jump;
				}*/
				return pesStatusTemp;
			}

		}
		else if ((m_nPESIndicator + PES_packet_length + sizeof(pes_header_t)) == m_nPSWrtiePos)
		{

			m_nPESLength = PES_packet_length + sizeof(pes_header_t);
			memcpy(m_pPESBuffer, m_pes, m_nPESLength);
			//清理m_nPESIndicator和m_nPSWrtiePos状态
			m_nPESIndicator = m_nPSWrtiePos = 0;
			m_status = ps_padding;

			pes_status_data_t pesStatusTemp;

			pesStatusTemp.pesTrued = true;
			pesStatusTemp.pesType = ps_pes_last;
			pesStatusTemp.pesPtr = (pes_header_t*)m_pPESBuffer;

			return pesStatusTemp;

		}
		else
		{
			m_nPESIndicator = m_nPSWrtiePos = 0;
			m_status = ps_padding;
			printf("ERROR： never go here unless it lost some data, the ps packet should be discarded\n");
		}
	}

	pes_status_data_t pesStatusTemp;

	pesStatusTemp.pesTrued = false;
	pesStatusTemp.pesType = ps_pes_lost;
	pesStatusTemp.pesPtr = NULL;

	return pesStatusTemp;

}

bool CRTPtoES::naked_payload()
{
	bool achieved = false;
	do
	{

		pes_status_data_t t = pes_payload();
		if (!t.pesTrued)
		{
			break;
		}
		PSStatus status = t.pesType;

		pes_header_t* pes = t.pesPtr;
		optional_pes_header* option = (optional_pes_header*)((char*)pes + sizeof(pes_header_t));
		if (option->PTS_DTS_flags != 2 && option->PTS_DTS_flags != 3 && option->PTS_DTS_flags != 0)
		{
			::fprintf(stderr, "PTS_DTS_flags is 01 which is invalid PTS_DTS_flags\n");
			break;
		}
		unsigned __int64 pts = get_pts(option);
		unsigned __int64 dts = get_dts(option);
		unsigned char stream_id = pes->stream_id;
		unsigned short PES_packet_length = ntohs(pes->PES_packet_length);
		char* pESBuffer = ((char*)option + sizeof(optional_pes_header)+option->PES_header_data_length);
		int nESLength = PES_packet_length - (sizeof(optional_pes_header)+option->PES_header_data_length);
		//delete the sei information
		//if ((stream_id == 0xE0) && ((sei_Nal == is_264_param(pESBuffer))))
		//	continue;

		memcpy(m_pESBuffer + m_nESLength, pESBuffer, nESLength);
		m_nESLength += nESLength;

		// sps、pps、sei和I帧作为一个单元输出
		if ((stream_id == 0xE0) && ((sps_Nal == is_264_param(pESBuffer)) || (pps_Nal == is_264_param(pESBuffer)) || (sei_Nal == is_264_param(pESBuffer))))
		{
			m_bFirstIFrame = true;
			achieved = true;
		}
		else if (stream_id == 0xE0)
		{
			//此处采用回调将数据返回
			esFrameData_t *esFrmDTemp = (esFrameData_t *)malloc(sizeof(esFrameData_t));
			esFrmDTemp->mediaType = VEDIO_T;
			esFrmDTemp->dts = dts;
			esFrmDTemp->pts = pts;
			esFrmDTemp->frame_len = m_nESLength;
			esFrmDTemp->frame_data = m_pESBuffer;
			//回调，将数据输出
			if (m_bFirstIFrame)
			{
				m_getEsData(esFrmDTemp);
			}
			
			free(esFrmDTemp);
			m_nESLength = 0;

			achieved = true;

		}
		else if (stream_id == 0xC0)
		{
			//此处采用回调将数据返回
			esFrameData_t *esFrmDTemp = (esFrameData_t *)malloc(sizeof(esFrameData_t));
			esFrmDTemp->mediaType = AUDIO_T;
			esFrmDTemp->dts = dts;
			esFrmDTemp->pts = pts;
			esFrmDTemp->frame_len = m_nESLength;
			esFrmDTemp->frame_data = m_pESBuffer;
			//回调，将数据输出
			if (m_bFirstIFrame)
			{
				m_getEsData(esFrmDTemp);
			}

			free(esFrmDTemp);
			m_nESLength = 0;

			achieved = true;
		}
		else
		{
			fprintf(stderr, "the pes packet is some private data without parsing\n");
			m_nESLength = 0;
		}

		//如果是此ps包的最后一个pes包，则退出循环
		if (ps_pes_last == status)
		{
			break;
		}


	} while (true);

	return achieved;

}


//test the class
#define TEST_MAIN 1
#if TEST_MAIN

#pragma comment(lib,"ws2_32.lib")

unsigned int Videocnt = 0;
unsigned int Audiocnt = 0;
void getAVData(esFrameData_t *fdata);


int main(int argc, char **argv)
{
#if defined(WIN32) && defined(_DEBUG) && defined(LEAK_CHECK)
	FindMemoryLeaks fml;
	{
#endif
		::printf("Hello, achilles_xu\n");
		//测试状态，需要从文件里面读取，实际的rtp数据包

		FILE *fp = NULL;
		char fileName[50] = { 0 };
		int fileCnt = 176;
		int fileSize = 0;

		char *rtpDataPtr = NULL;

		bool successed = false;

		CRTPtoES *getPSp = new CRTPtoES(getAVData);

		for (int i = 176; i < 1236; i++)
		{
			//read data from file 
			sprintf_s(fileName, 50, "./CGB2818/CGBT28181DataGetPs_%d", fileCnt);
			if (fopen_s(&fp, fileName, "rb") != 0){
				fprintf(stderr, "open rtp %s file failed\n", fileName);
				return 0;
			}
			::fseek(fp, 0, SEEK_END);
			fileSize = ftell(fp);
			::fseek(fp, 0, SEEK_SET);
			//
			rtpDataPtr = (char *)malloc(fileSize + 1);
			::memset(rtpDataPtr, 0, fileSize + 1);
			::fread(rtpDataPtr, 1, fileSize, fp);
			::printf("read fileCnt = %d\n", fileCnt);


			successed = getPSp->inPutRTPData(rtpDataPtr, fileSize);
			if (!successed){
				//关闭文件，释放内存
				::free(rtpDataPtr);
				fileSize = 0;

				::fclose(fp);
				fp = NULL;

				::printf("when we read the number %d file, a error comes and stop ", fileCnt);
				break;

			}

			::free(rtpDataPtr);
			fileSize = 0;

			::fclose(fp);
			fp = NULL;
			fileCnt++;

		}
		delete getPSp;
		::printf("Game over, achilles_xu\n");

#if defined(WIN32) && defined(_DEBUG) && defined(LEAK_CHECK)
	}
#endif
	return 0;
}

void getAVData(esFrameData_t *fdata)
{
	FILE *fp;
	char fileName[50] = { 0 };

	if (VEDIO_T == fdata->mediaType)
	{
		sprintf_s(fileName, 50, "Video-%d.vFrame", Videocnt);
		Videocnt++;
	}
	else if (AUDIO_T == fdata->mediaType)
	{
		sprintf_s(fileName, 50, "Audio-%d.aFrame", Audiocnt);
		Audiocnt++;
	}
	
	if (fopen_s(&fp, fileName, "wb") != 0){
		fprintf(stderr, "open rtp %s file failed\n", fileName);
		return;
	}

	//写音视频帧
	fwrite(fdata->frame_data,1,fdata->frame_len,fp);

	fclose(fp);
	
	printf("ok\n");
}


#endif


