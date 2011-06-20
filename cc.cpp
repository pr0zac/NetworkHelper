#include <cmath>
#include <cstring>
#include "core.h"
#include "cc.h"

ZUDTCC::ZUDTCC():
m_iRCInterval(),
m_LastRCTime(),
m_bSlowStart(),
m_iLastAck(),
m_bLoss(),
m_iLastDecSeq(),
m_dLastDecPeriod(),
m_iNAKCount(),
m_iDecRandom(),
m_iAvgNAKNum(),
m_iDecCount()
{
}

void ZUDTCC::init()
{
	m_iRCInterval = m_iSYNInterval;
	m_LastRCTime = CTimer::getTime();
	setACKTimer(m_iRCInterval);
	
	m_bSlowStart = false;
	m_iLastAck = m_iSndCurrSeqNo;
	m_bLoss = false;
	m_iLastDecSeq = CSeqNo::decseq(m_iLastAck);
	m_dLastDecPeriod = 1;
	m_iAvgNAKNum = 0;
	m_iNAKCount = 0;
	m_iDecRandom = 1;
	
//	m_iMSS = 32768;
//	m_dMaxCWndSize = 1600000000;
	m_dCWndSize = 16;
	m_dCWndModifier = 16;
	m_dPktSndPeriod = 1;
}

void ZUDTCC::onACK(const int32_t& ack)
{
	uint64_t currtime = CTimer::getTime();
	if (currtime - m_LastRCTime < (uint64_t)m_iRCInterval)
		return;
	
	m_LastRCTime = currtime;
	
	if (m_bSlowStart)
	{
		m_dCWndSize += CSeqNo::seqlen(m_iLastAck, ack);
		m_iLastAck = ack;
		
		if (m_dCWndSize > m_dMaxCWndSize)
		{
			m_bSlowStart = false;
			if (m_iRcvRate > 0)
				m_dPktSndPeriod = 1000000.0 / m_iRcvRate;
			else
				m_dPktSndPeriod = m_dCWndSize / (m_iRTT + m_iRCInterval);
		}
	}
	else
	{
		m_dCWndSize = m_iRcvRate / 1000000.0 * (m_iRTT + m_iRCInterval) + m_dCWndModifier;
		m_dCWndModifier++;
//		fprintf(stdout, "Increasing window modifier: %f\n", m_dCWndModifier);
		//fprintf(stdout, "m_dCWndSize: %f  m_iRcvRate: %d  m_iRTT: %d  m_iRCInterval: %d\n", m_dCWndSize, m_iRcvRate, m_iRTT, m_iRCInterval);
	}
	
	// During Slow Start, no rate increase
	if (m_bSlowStart)
		return;
	
	if (m_bLoss)
	{
		m_bLoss = false;
		return;
	}
	
	int64_t B = (int64_t)(m_iBandwidth - 1000000.0 / m_dPktSndPeriod);
	if ((m_dPktSndPeriod > m_dLastDecPeriod) && ((m_iBandwidth / 9) < B))
		B = m_iBandwidth / 9;
	//fprintf(stdout, "B: %lld  m_iBandwidth: %d\n", B, m_iBandwidth);
	
	double inc;
	
	if (B <= 0)
		inc = 1.0 / m_iMSS;
	else
	{
		// inc = max(10 ^ ceil(log10( B * MSS * 8 ) * Beta / MSS, 1/MSS)
		// Beta = 1.5 * 10^(-6)
		
		inc = pow(10.0, ceil(log10(B * m_iMSS * 8.0))) * 0.0000015 / m_iMSS;
		
		if (inc < 1.0/m_iMSS)
			inc = 1.0/m_iMSS;
	}
	
	m_dPktSndPeriod = (m_dPktSndPeriod * m_iRCInterval) / (m_dPktSndPeriod * inc + m_iRCInterval);
	//fprintf(stdout, "m_dPktSndPeriod: %f  inc: %f\n", m_dPktSndPeriod, inc);
	
	//set maximum transfer rate
	if ((NULL != m_pcParam) && (m_iPSize == 8))
	{
		int64_t maxSR = *(int64_t*)m_pcParam;
		if (maxSR <= 0)
			return;
		
		double minSP = 1000000.0 / (double(maxSR) / m_iMSS);
		if (m_dPktSndPeriod < minSP)
		{
			m_dPktSndPeriod = minSP;
		}
	}
}

void ZUDTCC::onLoss(const int32_t* losslist, const int&)
{
	if (m_dCWndModifier > 16)
	{
		m_dCWndModifier--;
	}
//	fprintf(stdout, "Decreasing window modifier: %f\n", m_dCWndModifier);
	//Slow Start stopped, if it hasn't yet
	if (m_bSlowStart)
	{
		m_bSlowStart = false;
		if (m_iRcvRate > 0)
			m_dPktSndPeriod = 1000000.0 / m_iRcvRate;
		else
			m_dPktSndPeriod = m_dCWndSize / (m_iRTT + m_iRCInterval);
	}
	
	m_bLoss = true;
	if (CSeqNo::seqcmp(losslist[0] & 0x7FFFFFFF, m_iLastDecSeq) > 0)
	{
		m_dLastDecPeriod = m_dPktSndPeriod;
		m_dPktSndPeriod = ceil(m_dPktSndPeriod * 1.125);
		
		m_iAvgNAKNum = (int)ceil(m_iAvgNAKNum * 0.875 + m_iNAKCount * 0.125);
		m_iNAKCount = 1;
		m_iDecCount = 1;
		
		m_iLastDecSeq = m_iSndCurrSeqNo;
		
		// remove global synchronization using randomization
		srand(m_iLastDecSeq);
		m_iDecRandom = (int)ceil(m_iAvgNAKNum * (double(rand()) / RAND_MAX));
		if (m_iDecRandom < 1)
			m_iDecRandom = 1;
	}
	else if ((m_iDecCount ++ < 5) && (0 == (++ m_iNAKCount % m_iDecRandom)))
	{
		// 0.875^5 = 0.51, rate should not be decreased by more than half within a congestion period
		m_dPktSndPeriod = ceil(m_dPktSndPeriod * 1.125);
		m_iLastDecSeq = m_iSndCurrSeqNo;
	}
}

void ZUDTCC::onTimeout()
{
	if (m_bSlowStart)
	{
		m_bSlowStart = false;
		if (m_iRcvRate > 0)
			m_dPktSndPeriod = 1000000.0 / m_iRcvRate;
		else
			m_dPktSndPeriod = m_dCWndSize / (m_iRTT + m_iRCInterval);
	}
	else
	{
		/*
		 m_dLastDecPeriod = m_dPktSndPeriod;
		 m_dPktSndPeriod = ceil(m_dPktSndPeriod * 2);
		 m_iLastDecSeq = m_iLastAck;
		 */
	}
}

void ZUDTCC::setBW(int64_t bw)
{
	this->setUserParam((char*)&(bw), sizeof(bw));
}
