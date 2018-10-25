
#ifndef LOOKUPTABLE_UTF_H
#define LOOKUPTABLE_UTF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <intrin.h>
#include <set>
#include <map>
#include <vector>


#ifdef _DEBUG
#define debugLog printf
#else
static void printNothingHack(...) {}
#define debugLog printNothingHack
#endif

/* Source https://lemire.me/blog/2018/05/09/how-quickly-can-you-check-that-a-string-is-valid-unicode-utf-8/
*  00-7F
*  C2-DF   80-BF
*  E0      A0-BF   80-BF
*  E1-EC   80-BF   80-BF
*  ED      80-9F   80-BF
*  EE-EF   80-BF   80-BF
*  F0      90-BF   80-BF   80-BF
*  F1-F3   80-BF   80-BF   80-BF
*  F4      80-8F   80-BF   80-BF
*/


enum eDecodeState
{
	INVALID_STATE,
	READY_NEW,
	EXPECT_80BF, 
	THREE_START_0,
	THREE_START_2,
	FOUR_START_0,
	FOUR_START_1,
	FOUR_START_2,
	EXPECT_2X_80BF,
	MAX_DECODE_STATE,
};


static eDecodeState singleByteDecode(uint8_t charVal, eDecodeState runVal)
{
	switch (runVal)
	{
		case INVALID_STATE:
		{
			return INVALID_STATE;
		}
		case 	READY_NEW:
		{
			if (charVal <= 0x7F)
			{
				return READY_NEW;// single ascii
			}
			else if (charVal >= 0xC2 && charVal <= 0xDF)
			{
				return EXPECT_80BF;
			}
			else if (charVal == 0xE0)
			{
				return THREE_START_0;
			}
			else if (charVal >= 0xE1 && charVal <= 0xEC)
			{
				return EXPECT_2X_80BF;
			}
			else if (charVal == 0xED)
			{
				return THREE_START_2;
			}
			else if (charVal >= 0xEE && charVal <= 0xEF)
			{
				return EXPECT_2X_80BF;
			}
			else if (charVal == 0xF0)
			{
				return FOUR_START_0;
			}
			else if (charVal >= 0xF1 && charVal <= 0xF3)
			{
				return FOUR_START_1;
			}
			else if (charVal == 0xF4)
			{
				return FOUR_START_2;
			}
			else
			{
				return INVALID_STATE;
			}
		}
		case 	EXPECT_80BF:
		{
			if (charVal >= 0x80 && charVal <= 0xBF)
			{
				return READY_NEW;
			}
			else
			{
				return INVALID_STATE;
			}
		}
		case 	THREE_START_0:
		{
			if (charVal >= 0xA0 && charVal <= 0xBF)
			{
				return EXPECT_80BF;
			}
			else
			{
				return INVALID_STATE;
			}
		}
		case 	THREE_START_2:
		{
			if (charVal >= 0x80 && charVal <= 0x9F)
			{
				return EXPECT_80BF;
			}
			else
			{
				return INVALID_STATE;
			}
		}
		case 	FOUR_START_0:
		{
			if (charVal >= 0x90 && charVal <= 0xBF)
			{
				return EXPECT_2X_80BF;
			}
			else
			{
				return INVALID_STATE;
			}
		}
		case 	FOUR_START_1:
		{
			if (charVal >= 0x80 && charVal <= 0xBF)
			{
				return EXPECT_2X_80BF;
			}
			else
			{
				return INVALID_STATE;
			}
		}
		case 	FOUR_START_2:
		{
			if (charVal >= 0x80 && charVal <= 0x8F)
			{
				return EXPECT_2X_80BF;
			}
			else
			{
				return INVALID_STATE;
			}
		}
		case 	EXPECT_2X_80BF:
		{
			if (charVal >= 0x80 && charVal <= 0xBF)
			{
				return EXPECT_80BF;
			}
			else
			{
				return INVALID_STATE;
			}
		}
	}

	return INVALID_STATE;
}

static eDecodeState twoByteDecode(uint16_t char2, eDecodeState runVal)
{
	return singleByteDecode(char2 >> 8, singleByteDecode(char2 &0xFF, runVal));
}

#define TABLE_LOOKUP_USE_SLOW_VALIDATE 0

#if TABLE_LOOKUP_USE_SLOW_VALIDATE
struct ValidateSlowDecodeUTF8Obj
{
	ValidateSlowDecodeUTF8Obj()
	{
		assert(MAX_DECODE_STATE <= (1 << 4)); // 16 perm max
		tableDecode.resize(1 << (4 + 8));
		for (int i = 0; i < 256; i++)
		{
			for (int j = INVALID_STATE; j < MAX_DECODE_STATE; j++)
			{
				uint32_t finalLoc = (j << 8) | i;
				tableDecode[finalLoc] = (uint8_t) singleByteDecode(i, (eDecodeState) j);
			}
		}
	}

	uint8_t singleByteDecodeObj(uint32_t charVal, uint32_t runVal)
	{
		return   tableDecode[charVal | (runVal << 8)];
	}

	inline bool validateUTFFull(const char* src, size_t len)
	{
		uint8_t runState = READY_NEW;
		while (len > 0)
		{
			runState = singleByteDecodeObj( (uint8_t)*src++, runState);
			len--;
		}

		return runState == READY_NEW;
	}

	std::vector<uint8_t> tableDecode;
};


static ValidateSlowDecodeUTF8Obj singleByteDecoder;
#endif


#define LOOKUPTABLE_UTF8_SLOW 1
static bool validate_utf8_lookupTableSlow(const char *src, size_t len)
{
#if TABLE_LOOKUP_USE_SLOW_VALIDATE
	return singleByteDecoder.validateUTFFull(src, len);
#else
	eDecodeState runState = READY_NEW;
	while (*src != '\0' && len > 0)
	{
		runState = singleByteDecode(*src, runState);
		len--;
		src++;
	}

	return runState == READY_NEW;
#endif
}
static const int unqSizeBits = 6; // we expect 52 unique superposition states in total
static const int uniqueCountBits = 4; // eDecodeState all fit

static void PrecomputeTables(std::vector<uint8_t>& outTableDecode, std::vector<uint8_t>& outTableConcate, std::vector<uint8_t>& outTableJoin)
{
	std::set<std::set< std::pair< eDecodeState, eDecodeState> > > uniqueInOutSeqs;
	debugLog("Creating initial uniqueInOutSeqs\n");
	// every single possible pure decode input [0..0xFFFF]
	for (uint64_t k = 0; k < (1 << 16); k++)
	{
		std::set<std::pair< eDecodeState, eDecodeState> >  setForBehavior;
		for (int i = INVALID_STATE; i < MAX_DECODE_STATE; i++)
		{
			auto decodStateOp = twoByteDecode(k, (eDecodeState)i);
			setForBehavior.insert({ (eDecodeState)i, decodStateOp });
		}
		uniqueInOutSeqs.insert(setForBehavior);
	}
	

#if 0
	for (auto&& eachset : uniques)
	{
		for (auto&& eachInput : eachset)
		{
			debuglog("%d , %d     ", eachInput.first, eachInput.second);
		}
		debuglog("\n");
	}
#endif


	debugLog("Finding all joins...\n");
	static const int numSelfIter = 1; // when making sure this has all possible sequences use larger number
	for (int ii = 0; ii < numSelfIter; ii++)
	{
		std::set<std::set< std::pair< eDecodeState, eDecodeState> > >  uniqueInOutSeqs_join = uniqueInOutSeqs;
		for (auto&& eachInSet : uniqueInOutSeqs)
		{
			// look up map this eachSet0 through the vector
			// going FROM output of eachSet
			for (auto&& eachOutSet : uniqueInOutSeqs)
			{
				eDecodeState outerIn;
				eDecodeState outerOut;
				std::set<std::pair< eDecodeState, eDecodeState> >  setForBehavior;
				for (auto&&eachIn : eachInSet)
				{
					for (auto&&eachOut : eachOutSet)
					{
						if (eachIn.second == eachOut.first)
						{
							outerIn = eachIn.first;
							outerOut = eachOut.second;
							setForBehavior.insert({ outerIn, outerOut });
							break;
						}

					}
				}
				uniqueInOutSeqs_join.insert(setForBehavior);
			}
		}
		uniqueInOutSeqs = uniqueInOutSeqs_join;
	}
#if 0
	debuglog("StartPrint: \n");
	for (auto&& eachset : uniqueInOutSeqs)
	{
		for (auto&& eachInput : eachset)
		{
			debuglog("%d , %d     ", eachInput.first, eachInput.second);
		}
		debuglog("\n");
	}
#endif

	assert(uniqueInOutSeqs.size() <= (1<< unqSizeBits));

	outTableConcate.clear();
	outTableConcate.resize(1 << (unqSizeBits + 4));

	int uniqueNum = 0;
	for (auto&& eachset : uniqueInOutSeqs)
	{
		for (auto&& eachInput : eachset)
		{
			uint16_t  finalVal = (uniqueNum<<4) | (eachInput.first);
			outTableConcate[finalVal] = eachInput.second;// mapping full input to putput
		}
		uniqueNum++;
	}

	debugLog("Creating raw decoder...\n");
	outTableDecode.clear();
	outTableDecode.resize(1 << 16);
	// every single possible pure decode input [0..0xFFFF]
	for (uint32_t k = 0; k < (1 << 16); k++) 
	{
		std::set<std::pair< eDecodeState, eDecodeState> >  setForBehavior;
		for (int i = INVALID_STATE; i < MAX_DECODE_STATE; i++)
		{
			auto decodStateOp = twoByteDecode(k, (eDecodeState)i);
			setForBehavior.insert({ (eDecodeState)i, decodStateOp });
		}

		int uniqueNum = 0;
		for (auto&& eachset : uniqueInOutSeqs)
		{
			if (eachset == setForBehavior)
				break;

			uniqueNum++;
		}
		outTableDecode[k] = uniqueNum;
	}

	debugLog("Creating Join Table...\n");

	outTableJoin.clear();
	outTableJoin.resize(1 << (unqSizeBits + unqSizeBits));
	int inSetNum = 0;
	for (auto&& eachInSet : uniqueInOutSeqs)
	{
		// look up map this eachSet0 through the vector
		// going FROM output of eachSet
		int outSetNum = 0;
		for (auto&& eachOutSet : uniqueInOutSeqs)
		{
			eDecodeState outerIn;
			eDecodeState outerOut;
			std::set<std::pair< eDecodeState, eDecodeState> >  setForBehavior;
			for (auto&&eachIn : eachInSet)
			{
				for (auto&&eachOut : eachOutSet)
				{
					if (eachIn.second == eachOut.first)
					{
						outerIn = eachIn.first;
						outerOut = eachOut.second;
						setForBehavior.insert({ outerIn, outerOut });
						break;
					}

				}
			}

			int uniqueNum = 0;
			for (auto&& eachset : uniqueInOutSeqs)
			{
				if (eachset == setForBehavior)
					break;

				uniqueNum++;
			}

			uint32_t finalLoc = (inSetNum << unqSizeBits) | outSetNum;
			outTableJoin[finalLoc] = uniqueNum;
			outSetNum++;
		}
		inSetNum++;
	}
}

struct ValidateFastDecodeUTF8Obj
{
	ValidateFastDecodeUTF8Obj()
	{
		PrecomputeTables(tableDecode, tableConcate, tableJoin);
	}


	inline bool validateUTFFull(const char* src, size_t len)
	{
		eDecodeState runState = READY_NEW;
		while ((len & 0x7) != 0) // till length is divisible by 8
		{
			runState = singleByteDecode( *src++, runState);
			len--;
			if (len == 0)
			{
				return runState == READY_NEW;
			}
		}

		// alignment here likely doesnt matter https://lemire.me/blog/2012/05/31/data-alignment-for-speed-myth-or-reality/
		const uint16_t* asShort = reinterpret_cast<const uint16_t*>(src);
		const uint16_t* asShortEnd = &asShort[len / sizeof(uint16_t)];
		uint8_t smallCon = runState;
		while (asShort != asShortEnd)
		{
			uint8_t orig0 = tableDecode[*asShort++];
			uint8_t orig1 = tableDecode[*asShort++];;
			uint8_t orig2 = tableDecode[*asShort++];;
			uint8_t orig3 = tableDecode[*asShort++];;
			uint8_t prejoin0 = tableJoin[orig0 << unqSizeBits | orig1];
			uint8_t prejoin1 = tableJoin[orig2 << unqSizeBits | orig3];
			uint8_t toCon3 = tableJoin[prejoin0 << unqSizeBits | prejoin1];
			smallCon = tableConcate[smallCon | (toCon3 << uniqueCountBits)];
		}

		return smallCon == READY_NEW;
	}

	std::vector<uint8_t> tableDecode;
	std::vector<uint8_t> tableJoin;
	std::vector<uint8_t> tableConcate;
};

static ValidateFastDecodeUTF8Obj fastValidateObj;

#define LOOKUPTABLE_UTF8_FAST 1
__declspec(noinline) static bool validate_utf8_lookupTableFast(const char *src, size_t len)
{
	return fastValidateObj.validateUTFFull(src, len);
}


#endif
