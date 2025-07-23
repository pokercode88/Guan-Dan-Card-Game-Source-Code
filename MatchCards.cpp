#include <string.h>
#include "MatchCards.h"

MatchCards::MatchCards()
{
    m_lCardsCount = 0;
}

long MatchCards::GetMatchCars(char *lpszCards, unsigned short usCount)
{
    long lResult = 0;

    if ((NULL != lpszCards) && (usCount > 0) && (m_lCardsCount == usCount))
    {
        memcpy(lpszCards, m_szCards, usCount);
        lResult = m_lCardsCount;
    }

    return lResult;
}

void MatchCards::OnGameMatchCardsMsg(void *buffer, long length)
{
    if ((NULL != buffer) && (sizeof(GameMatchCardsMsg) >= length))
    {
        GameMatchCardsMsg *lpMatchCardsMsg = (GameMatchCardsMsg*)buffer;

        memset(m_szCards, 0, sizeof(m_szCards));
        memcpy(m_szCards, lpMatchCardsMsg->szCards, lpMatchCardsMsg->usCount);
        m_lCardsCount = lpMatchCardsMsg->usCount;
    }
}
