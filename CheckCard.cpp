#include "CheckCard.h"

#include <algorithm>
#include <vector>
using namespace std;

//#define LITTLE_JOKCER              25

void CheckCard::JockerSwitch(int &jocker)       //因为A与2、2与鬼牌之间不连续，要进度转换
{
    if(jocker == 14)jocker = 18;
	 if(jocker == 15)jocker = 19;
    //if(jocker == 16)jocker = LITTLE_JOKCER;
    //if(jocker == 17)jocker = LITTLE_JOKCER+1;
}


int CheckCard::CheckShape(int arg_card[], int arg_num, int &arg_value, char level, int type)
{
	int i;
	int iRet;

	int temp[20] = {0};
	for(i =0; i<arg_num; i++)
	{
	    if(arg_card[i] != 0)
	    {
	        temp[i] = arg_card[i]&0x0f;
	    }
	}
	
	for(i = 0;i<arg_num;i++)
	{
	    if(temp[i] == 1 )
	    {
	        temp[i] = 14;
	    }
		else if(temp[i] == level)
	    {
	        temp[i] = 16;
	    }
		else if (14 == temp[i]) //小王
		{
			temp[i] = 18;
		}
		else if (15 == temp[i]) //大王
		{
			temp[i] = 19;
		}
	}
	
	iRet = JudgeCardShape(temp,arg_num,arg_value,arg_card,level,type);
	return iRet;
}


int CheckCard::JudgeCardShape(int arg_card[], int arg_num, int &arg_value,int arg_num_card[],char level, int type)
{
    if(arg_num <= 0)
    {
        arg_value = 0;
        return -1;
    }
	
	int m = 0;
	int n = 0;
	int countCard = 0;			//百搭牌数
	for( int k = 0 ; k < arg_num ; ++k)
	{

		if( (arg_card[k] == 16)	&&  ((arg_num_card[k] >> 4) == 2) && level != 1)
		{
			countCard++;
			if (countCard == 1)
			{
				m = k;
			}
			else
			{
				n = k;
			}
		}
		else if ( (arg_card[k] == 14)	&&  ((arg_num_card[k] >> 4) == 2) && level == 1)
		{
			countCard++;
			if (countCard == 1)
			{
				m = k;
			}
			else
			{
				n = k;
			}
		}
	}

	int index = 0;
	if (arg_num == 5)
	{
		for (int l = 0; l < 5; ++l)
		{
			if (arg_card[l] == 2 || arg_card[l] == 3 || arg_card[l] == 4 || arg_card[l] == 5)
			{
				index++;
			}
		}
	}
	bool is = false;
	int CardType ;
	if (countCard == 0)
	{
		return CheckCardShape(arg_card,arg_num,arg_value,arg_num_card,level,false);
	}
	else if (countCard == 1)
	{
		if (arg_num == 5)
		{
			if (arg_card[0] & 0x0f != 16 && level != 1)
			{
				CardType = arg_num_card[0] >> 4;
			}
			else if(arg_card[1] & 0x0f != 16 && level != 1)
			{
				CardType = arg_num_card[1] >> 4;
			}
			else if (arg_card[0] & 0x0f != 14 && level == 1)
			{
				CardType = arg_num_card[0] >> 4;
			}
			else if(arg_card[1] & 0x0f != 14 && level == 1)
			{
				CardType = arg_num_card[1] >> 4;
			}
			else
			{
				CardType = arg_num_card[2] >> 4;
			}
			int count = 0;
			for (int o = 0; o < 5 ; ++o)
			{
				if ( o == m)
				{
					continue;
				}
				if (CardType == arg_num_card[o] >> 4)
				{
					count++;
				}
			}

			if (count == 4)
			{
				is = true;
			}
		}
		for (int i = 16 ; i > 1 ; --i)
		{
			arg_card[m] = i;
			arg_num_card[m] = i;
			bool isSquence = false;
			if (i == 16)
			{
				arg_num_card[m] = level - 1;
				isSquence = true;
			}
			if ( i == 15 || (index == 4 && i == 14))
			{
				continue;
			}


			int temp = CheckCardShape(arg_card,arg_num,arg_value,arg_num_card,level,isSquence,is);
			if (temp != -1)
			{
				return temp;
			}
		}
	}
	else if (countCard == 2)
	{
		if ( arg_num == 5)
		{
			vector<int> vCard;
			for(int o = 0;o< arg_num;o++)
			{
				int temp = arg_card[o];
				vCard.push_back( temp);
			}
			sort(vCard.begin(), vCard.end());

			if ((vCard[2] > 16) || (vCard[3] == 18 && vCard[4] == 19)) //chw 1202
			{
				return 0;
			}
			if ( vCard[0] == vCard[1] && vCard[1] == vCard[2] && vCard[3] <= 16 && vCard[4] <= 16) //chw 1201
			{
				arg_value = vCard[0];
				return TYPE_BOMB_FIVE;
			}
			else if ( ((vCard[0] == vCard[1] && vCard[1] != vCard[2] && vCard[4] <= 16) || (vCard[0] != vCard[1] && vCard[1] == vCard[2] && vCard[4] <= 16)) && !(vCard[3] == 18 && vCard[4] == 19)) // chw 1203
			{
				arg_value = vCard[2];
				return TYPE_TRIAD_CARDS_PAIR;
			}
			/* chw 1203 begin */
			else if (vCard[3] == vCard[4] && vCard[3] > 16 && vCard[2] <= 16)
			{
				arg_value = vCard[0];
				return TYPE_TRIAD_CARDS_PAIR;
			}
			/* chw 1203 end */
			else
			{
				if (arg_card[0] & 0x0f != 16 && level != 1)
				{
					type = arg_num_card[0] >> 4;
				}
				else if (arg_card[0] & 0x0f != 16 && level != 1)
				{
					type = arg_num_card[1] >> 4;
				}
				else if (arg_card[0] & 0x0f != 16 && level != 1)
				{
					type = arg_num_card[2] >> 4;
				}
				else if (arg_card[0] & 0x0f != 14 && level == 1)
				{
					type = arg_num_card[0] >> 4;
				}
				else if (arg_card[0] & 0x0f != 14 && level == 1)
				{
					type = arg_num_card[1] >> 4;
				}
				else if (arg_card[0] & 0x0f != 14 && level == 1)
				{
					type = arg_num_card[2] >> 4;
				}
				else
				{
					type = arg_num_card[3] >> 4;
				}
				int count = 0;
				for (int o = 0; o < 5 ; ++o)
				{
					if ( o == m || o == n)
					{
						continue;
					}
					if (type == (arg_num_card[o] >> 4))
					{
						count++;
					}
				}

				if (count == 3)
				{
					is = true;
				}
				count = 0;
				for (int h = 0;h < 5; ++h)
				{
					if ((arg_card[h] == 2 || (level == 2 && arg_card[h] == 16)) || arg_card[h] == 3 || arg_card[h] == 4 || arg_card[h] == 5 )
					{
						count++;
					}
				}
				int temp = 0;
				for (int i = 14 ; i > 1 ; --i)
				{
					arg_card[m] = i;
					arg_num_card[m] = i;
					if (i == 14 && count >= 3)
					{
						continue;
					}
					for (int j = 14 ; j > 1 ; --j)
					{
						arg_card[n] = j;
						arg_num_card[n] = j;
						if (i == 14 && count >= 3)
						{
							continue;
						}
						temp = CheckSameSequence(arg_card,arg_num,arg_value,arg_num_card,level,is);
						if (temp != 0)
						{
							return temp;
						}
					}
				}
			}
		}
		else if ( arg_num == 6)
		{
			vector<int> vCard;
			for(int o = 0;o< arg_num;o++)
			{
				int temp = (arg_card[o]);
				vCard.push_back( temp);

			}
			sort(vCard.begin(), vCard.end());

			if (vCard[5] > 16)
			{
				arg_value = 0;
				return -1;
			}

			if (vCard[0] == vCard[1] && vCard[1] == vCard[2] && vCard[2] == vCard[3])
			{
				arg_value = vCard[0];
				return TYPE_BOMB_SIX;
			}
			else if ( (((vCard[0] == vCard[1] - 1) && vCard[1] == vCard[2] && vCard[2] == vCard[3] ) 
				|| ((vCard[1] == vCard[3] - 1) && vCard[0] == vCard[1] && vCard[1] == vCard[2])
				||	(vCard[0] == vCard[1] && vCard[2] == vCard[3] && (vCard[0] == vCard[2] - 1))
				||	(vCard[0] == 2 && vCard[0] == vCard[1] && vCard[1] == vCard[2] && vCard[3] == 14)
				||	(vCard[0] == 2 && vCard[1] == 14 && vCard[1] == vCard[2] && vCard[3] == vCard[2])
				||	(vCard[0] == vCard[1] && vCard[2] == vCard[3] && vCard[0] == 2 && vCard[3] == 14))
				&& type != TYPE_PAIR_SEQUENCE_CARDS)
			{
				arg_value = vCard[3];
				if (vCard[0] == 2)
				{
					arg_value = vCard[0];
				}
				return TYPE_TRIAD_SEQUENCE_CARDS;
			}
			else if (vCard[0] == 14 && vCard[1] == 14 && vCard[2] == 14 && vCard[3] == 16 && vCard[4] == 16 && vCard[5] == 16 && level == 2)
			{
				arg_value = 2;
				return TYPE_TRIAD_SEQUENCE_CARDS;
			}
			else if (vCard[0] == level - 1 && vCard[1] == level - 1 && vCard[2] == level - 1 && vCard[3] == 16 && vCard[4] == 16 && vCard[5] == 16 )
			{
				arg_value = level;
				return TYPE_TRIAD_SEQUENCE_CARDS;
			}
			else if (vCard[0] == level + 1 && vCard[1] == level + 1 && vCard[2] == level + 1 && vCard[3] == 16 && vCard[4] == 16 && vCard[5] == 16 )
			{
				arg_value = level + 1;
				return TYPE_TRIAD_SEQUENCE_CARDS;
			}
			else if (vCard[0] == vCard[1] && vCard[2] == vCard[3] && vCard[1] + 2 == vCard[2])
			{
				arg_value = vCard[3];
				return TYPE_PAIR_SEQUENCE_CARDS;
			}
			else if ((vCard[0] == vCard[1] && vCard[1] + 1 == vCard[2] && vCard[2] + 1 == vCard[3])
				||(vCard[2] == vCard[3] && vCard[0] + 1 == vCard[1] && vCard[1] + 1 == vCard[2])
				||(vCard[1] == vCard[2] && vCard[0] + 1 == vCard[1] && vCard[2] + 1 == vCard[3]))
			{
				arg_value = vCard[3];
				return TYPE_PAIR_SEQUENCE_CARDS;
			}
			else if ((vCard[0] == vCard[1] && vCard[2] == vCard[3] && vCard[1] + 1 == vCard[2]))
			{
				if (vCard[2] == 14)
				{
					arg_value = 14;
				}
				else
				{
					arg_value = vCard[3] + 1;
				}
				return TYPE_PAIR_SEQUENCE_CARDS;
			}
			else if ((vCard[0] == 2 && vCard[1] == 2 && vCard[2] == 14 && vCard[3] == 14)
				|| (vCard[0] == 2 && vCard[1] == 3 && vCard[2] == 3 && vCard[3] == 14)
				|| (vCard[0] == 2 && vCard[1] == 2 && vCard[2] == 3 && vCard[3] == 14)
				|| (vCard[0] == 3 && vCard[1] == 3 && vCard[2] == 14 && vCard[3] == 14)
				|| (vCard[0] == 2 && vCard[1] == 3 && vCard[2] == 14 && vCard[3] == 14))
			{
				arg_value = 3;
				return TYPE_PAIR_SEQUENCE_CARDS;
			}
		}
		else
		{
			int temp = 0;
			for (int i = 16 ; i > 1 ; --i)
			{
				arg_card[m] = i;
				arg_num_card[m] = i;
				if ( i == 15 )
				{
					continue;
				}
				for (int j = 16 ; j > 1 ; --j)
				{
					if ( j == 15 )
					{
						continue;
					}
					arg_card[n] = j;
					arg_num_card[n] = j;
					temp = CheckCardShape(arg_card,arg_num,arg_value,arg_num_card,level,false);
					if (temp != -1)
					{
						return temp;
					}
				}
			}
		}
	}
	arg_value = 0;
	return -1;
    
}

int CheckCard::CheckCardShape(int arg_card[], int arg_num, int &arg_value,int arg_num_card[],char level,bool isSequence,bool is)
{
	vector<int>card(arg_card, arg_card + arg_num);
	//for_each(card.begin(), card.end(), JockerSwitch);
	sort(card.begin(), card.end());
	for(int num=arg_num; num<27; num++)
		card.push_back(-1);

	int statu = 1;
	char more_statu = 0;
	int i=1;
	int temp_count = 1;
	int max_count =1;
	arg_value = card[0];

	while(card[i] != -1 && i < 27)
	{
		if(card[i] == card[i-1])
		{
			statu = statu|(3<<(i*2));
			temp_count ++;
		}
		else if(card[i] == card[i-1]+1)
		{
			statu = statu|(2<<(i*2));
			temp_count = 1;
		}
		else
		{
			statu = statu|(1<<(i*2));
			temp_count = 1;
		}

		if(temp_count >= max_count)
		{
			max_count = temp_count;
			arg_value = card[i];
		}
		i++;
	}

	if(more_statu == 0)
	{
		switch(statu)
		{
		case 237:
			if (LITTLE_JOKCER == card[0])
				return TYPE_ROCKET_CARD;
			else
			{
				arg_value = 0;
				return -1;
			}
			break;
			/*****-----------------单牌-------------*****/
		case 1:
			return TYPE_SINGLE_CARDS;
			break;
			/*****-----------------对子-------------*****/
		case 13:
			return TYPE_PAIR_CARDS;
			break;
			/*****-----------------三条-------------*****/
		case 61:
			return TYPE_TRIAD_CARDS;
			break;
			/*****--------------四枚(炸弹)------------*****/
		case 253:
			return TYPE_BOMB_CARD;
			break;

		case 1021: 
			return TYPE_BOMB_FIVE;
			break;

		case 4093:
			return TYPE_BOMB_SIX;
			break;

		case 16381:
			return TYPE_BOMB_SEVEN;
			break;

		case 65533:
			return TYPE_BOMB_EIGHT;
			break;
		case 262141:
			return TYPE_BOMB_NINE;
			break;
		case 1048573:
			return TYPE_BOMB_TEN;
			break;
			/*****-----------------三带二-------------*****/
		case 989:
		case 1005:
		case 957:
		case 893:
			return TYPE_TRIAD_CARDS_PAIR;
			break;
			/****----顺子(五张用及以上数值相邻的单牌)----*****/
		case 681:     
			{
				int tempType = arg_num_card[0]>>4;
				int n ;
				int m = 0;
				for (n = 1;n < 5 ; ++n)
				{
					if (arg_num_card[n]>>4 == tempType)
					{
						m++;
					}
				}
				if ( m == 4 || is)
				{
					return TYPE_SINGLE_SAMESEQUENCE_CARDS;
					break;
				}
				return TYPE_SINGLE_SEQUENCE_CARDS;  //5张
				break;
			}

			/****----三顺(两个及以上数值相邻的三条)----*****/
		case 4029:        
			return TYPE_TRIAD_SEQUENCE_CARDS;   //(连续2个)
			break;
			/****----双顺(两个及以上数值相邻的对子)----*****/
		case 3821:         
			return TYPE_PAIR_SEQUENCE_CARDS;//(3对)
			break;
		default:
			{
				int temp = CheckSameSequence(arg_card,arg_num,arg_value,arg_num_card,level,is);
				if ( temp != 0 && !isSequence)
				{
					return temp;
				}

				temp = CheckPairSequence(arg_card,arg_num,arg_value,arg_num_card,level);
				if ( temp != 0)
				{
					return temp;
				}

				temp = CheckTriadSequence(arg_card,arg_num,arg_value,arg_num_card,level);
				if ( temp != 0)
				{
					return temp;
				}

				arg_value = 0;
				return -1;
			}
		}
	}
	return -1;
}

int CheckCard::CheckSameSequence(int arg_card[], int arg_num, int &arg_value,int arg_num_card[] ,char level,bool is)
{
	if ( arg_num == 5)
	{
		vector<int> vCard;
		for( int m = 0;m< arg_num;m++)
		{
			if (arg_card[m] == 16)
			{
				vCard.push_back(level);
			}
			else
			{
				vCard.push_back(arg_card[m]);
			}
		}
		sort(vCard.begin(), vCard.end());

		if(vCard[0] == 2 && vCard[1] == 3 && vCard[2] == 4 && vCard[3] == 5 && vCard[4] == 14)   
		{
			arg_value = vCard[3];
			int tempType = arg_num_card[0]>>4;
			int n ;
			int m = 0;
			for (n = 1;n < 5 ; ++n)
			{
				if (arg_num_card[n]>>4 == tempType)
				{
					m++;
				}
			}
			if ( m == 4 || is)
			{
				return TYPE_SINGLE_SAMESEQUENCE_CARDS;
			}
			return TYPE_SINGLE_SEQUENCE_CARDS;
		}

		if(vCard[0] + 1 == vCard[1] && vCard[1] + 1 == vCard[2] && vCard[2] + 1 == vCard[3] && vCard[3] + 1 == vCard[4])   
		{
			//arg_value = vCard[4];
			//if (level == 2 && vCard[0] == 2)
			//{
			//	arg_value = vCard[4] + 1;
			//}
			//else
			{
				arg_value = vCard[4];
			}
			int tempType = arg_num_card[0]>>4;
			int n ;
			int m = 0;
			for (n = 1;n < 5 ; ++n)
			{
				if (arg_num_card[n]>>4 == tempType)
				{
					m++;
				}
			}
			if ( m == 4 || is)
			{
				return TYPE_SINGLE_SAMESEQUENCE_CARDS;
			}
			return TYPE_SINGLE_SEQUENCE_CARDS;
		}
	}
	return 0;
}



int CheckCard::CheckPairSequence(int arg_card[], int arg_num, int &arg_value,int arg_num_card[],char level)
{
	if ( arg_num == 6)
	{
		vector<int> vCard;
		for( int m = 0;m< arg_num;m++)
		{
			if (arg_card[m] == 16)
			{
				vCard.push_back(level);
			}
			else
			{
				vCard.push_back(arg_card[m]);
			}
		}
		sort(vCard.begin(), vCard.end());

		if (vCard[0] == vCard[1] && vCard[1] + 1 == vCard[2] && vCard[2] == vCard[3] && vCard[3] + 1 == vCard[4] && vCard[4] == vCard[5] )
		{
			arg_value = vCard[5];
			return TYPE_PAIR_SEQUENCE_CARDS;
		}

		if (vCard[0] == vCard[1] && vCard[1] + 1 == vCard[2] && vCard[2] == vCard[3] && vCard[0] == 2 && vCard[4] == 14 && vCard[5] == 14 )
		{
			arg_value = vCard[3];
			return TYPE_PAIR_SEQUENCE_CARDS;
		}
	}

	return 0;

}


int CheckCard::CheckTriadSequence(int arg_card[], int arg_num, int &arg_value,int arg_num_card[],char level)
{
	if ( arg_num == 6)
	{
		vector<int> vCard;
		for( int m = 0;m< arg_num;m++)
		{
			if (arg_card[m] == 16)
			{
				vCard.push_back(level);
			}
			else
			{
				vCard.push_back(arg_card[m]);
			}
		}
		sort(vCard.begin(), vCard.end());

		if (vCard[0] == vCard[1] && vCard[1] == vCard[2] && vCard[2] + 1 == vCard[3] && vCard[3] == vCard[4] && vCard[4] == vCard[5] )
		{
			arg_value = vCard[5];
			return TYPE_TRIAD_SEQUENCE_CARDS;
		}

		if (vCard[0] == vCard[1] && vCard[1] == vCard[2] && vCard[0] == 2 && vCard[3] == 14 && vCard[4] == 14 && vCard[5] == 14)
		{
			arg_value = vCard[0];
			return TYPE_TRIAD_SEQUENCE_CARDS;
		}
	}
	return 0;
}

