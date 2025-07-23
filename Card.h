#ifndef __CARD_H_
#define __CARD_H_


class Card
{
public:
typedef enum
{
        Spade,//黑桃
        Heart,//红心
        Club,       //梅花
        Diamond = 0,//方块
} CardType;

private:
    operator char() const;   // to avoid the use of { if(c>Card(x))}

public:
    Card(const Card& c);

    static char MakeCardChar(CardType type, char c);

    char AsChar();           //输出char类型表示，高4位放CardType,低4位放牌面A＝1，5＝5，小王＝16，大王＝17
    Card();
    CardType Type() const;   //获取类型
    char Name() const;       //牌面，A=1,2＝2，3＝3，... K=13，大王＝17，小王＝16
    char Value() const;      //牌的大小，3=3,A=14,2=15,...k=13,大王＝17，小王＝16
    ~Card();
    Card(char c);            //c应该是包含花色和牌面的
    Card(int type,char name);//从类型和牌面创建
    Card& operator =(const Card& c);
    bool operator >(const Card& c) const;
    bool operator >=(const Card& c) const;
    bool operator <(const Card& c) const;
    bool operator <=(const Card& c) const;
    bool operator ==(const Card& c) const;

private:
    char m_card;//前面4位表示花色，后面4位表示大小
};

#endif /* __CARD_H_ */
