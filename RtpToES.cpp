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
	//�ͷ���Դ
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
	//��ȡrtp��sequence number
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
	//������rtp���У��Ƿ����ps��ͷ�����Ǽ���Ƿ��� 00 00 01 BA
	//������ʱ������
	ps_header_t *tempPs = NULL;
	unsigned int  mPackIndicator = 0;
	//�������������ܱȽϺķ�ʱ��,
	//��ʱֻ����ǰ����֮һ��λ��
	for (; mPackIndicator<length / 3; mPackIndicator++)
	{

		tempPs = (ps_header_t*)(data + mPackIndicator);

		if (is_ps_header(tempPs))
		{
			//����ҵ����λ
			tempPsRtpData->ps_headered = true;
			//���
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

	
	//ps_rtp_data_t�ṹ������
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
	
	if (3 == m_nPsIndicator)//�ﵽҪ��set������������������00 00 01 BA,�������һ��PS֡
	{
		::printf("ready to output a complete PS frame\n");
		if (!m_bAshBeforeFirst)
		{
			//ɾ����һ��00 00 01 BA��־��ǰ�����ݰ�
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

	//���PS֡���Ƿ��ж���
	
	if (false == (*m_sDataSet.begin())->ps_headered)
	{
		fprintf(stderr, "the packet including ps_header lost! sequence is %d   \n", (*m_sDataSet.begin())->seq_number);
		return achieved;
	}

	uint32_t tPSFrmLen = 0;//��ʱ��¼m_nPSFrmLen����
	uint16_t tPSCompleted = 0;//��ʱ��¼������ps���Ƿ��������������Ļ���Ҫ����
	m_nPSWrtiePos = 0;


	memcpy((m_pPSBuffer + tPSFrmLen), ((*m_sDataSet.begin())->rtp_data_ptr), (*m_sDataSet.begin())->rtp_len);
	tPSFrmLen += (*m_sDataSet.begin())->rtp_len;
	//��¼��һ��rtp����sequence number
	uint16_t tempLastSeq = (*m_sDataSet.begin())->seq_number;
	//tPSCompleted += (*m_sDataSet.begin())->ps_completed;

	set<ps_rtp_data_t *, com_Seq>::iterator itFirst = m_sDataSet.begin();
	free(*itFirst);
	m_sDataSet.erase(itFirst++);//���������ƶ�����һ��rtp����λ��

	
	for (set<ps_rtp_data_t *, com_Seq>::iterator it = m_sDataSet.begin(); it != m_sDataSet.end();)
	{
		if ((*it)->seq_number == m_arrRtpSeq[1].seq_number)
		{
			if ((tempLastSeq + 1) == m_arrRtpSeq[1].seq_number)
			{
				tPSCompleted = 1;
			}
			//���ps֡��ֻ����һ��rtp��
			break;
		}
		//rtp���Ѿ��Ƴ�ͷ��12�ֽ�

		if (((tempLastSeq + 1) == (*it)->seq_number))
		{
			memcpy((m_pPSBuffer + tPSFrmLen), ((*it)->rtp_data_ptr), (*it)->rtp_len);
			tPSFrmLen += (*it)->rtp_len;
			//tPSCompleted += (*it)->ps_completed;
			tempLastSeq = (*it)->seq_number;
			//�Ƴ��Ѿ����Ƶ�����
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

	//����״̬���
	m_nPsIndicator--;
	m_arrRtpSeq[0].seq_number = m_arrRtpSeq[1].seq_number;
	m_arrRtpSeq[1].seq_number = m_arrRtpSeq[2].seq_number;

	m_arrRtpSeq[2].seq_number = 0;
	m_arrRtpSeq[2].used = false;
	
	if ((false == discarded) && (1==tPSCompleted))
	{
		m_nPSWrtiePos = tPSFrmLen;
		::printf("PS frame length  is: %d\n", m_nPSWrtiePos);
		//������һ����ps֡�Ľ�������
		//����naked_payload
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
	//��Ϊ������ps������ͷ�ֽڿ϶���00 00 01 BA
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
				m_status = ps_psm;//���s_sh״̬
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
		//Ѱ����һ��pes

		unsigned short PES_packet_length = ntohs(m_pes->PES_packet_length);
		//���滹��pes��Ҫ����
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
			//����m_nPESIndicator��m_nPSWrtiePos״̬
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
			printf("ERROR�� never go here unless it lost some data, the ps packet should be discarded\n");
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

		// sps��pps��sei��I֡��Ϊһ����Ԫ���
		if ((stream_id == 0xE0) && ((sps_Nal == is_264_param(pESBuffer)) || (pps_Nal == is_264_param(pESBuffer)) || (sei_Nal == is_264_param(pESBuffer))))
		{
			m_bFirstIFrame = true;
			achieved = true;
		}
		else if (stream_id == 0xE0)
		{
			//�˴����ûص������ݷ���
			esFrameData_t *esFrmDTemp = (esFrameData_t *)malloc(sizeof(esFrameData_t));
			esFrmDTemp->mediaType = VEDIO_T;
			esFrmDTemp->dts = dts;
			esFrmDTemp->pts = pts;
			esFrmDTemp->frame_len = m_nESLength;
			esFrmDTemp->frame_data = m_pESBuffer;
			//�ص������������
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
			//�˴����ûص������ݷ���
			esFrameData_t *esFrmDTemp = (esFrameData_t *)malloc(sizeof(esFrameData_t));
			esFrmDTemp->mediaType = AUDIO_T;
			esFrmDTemp->dts = dts;
			esFrmDTemp->pts = pts;
			esFrmDTemp->frame_len = m_nESLength;
			esFrmDTemp->frame_data = m_pESBuffer;
			//�ص������������
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

		//����Ǵ�ps�������һ��pes�������˳�ѭ��
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
		//����״̬����Ҫ���ļ������ȡ��ʵ�ʵ�rtp���ݰ�

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
				//�ر��ļ����ͷ��ڴ�
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

	//д����Ƶ֡
	fwrite(fdata->frame_data,1,fdata->frame_len,fp);

	fclose(fp);
	
	printf("ok\n");
}


#endif


