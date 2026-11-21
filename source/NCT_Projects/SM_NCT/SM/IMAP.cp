/*------------------------------------------------------------------------------
	IMAP response parsing functions.
------------------------------------------------------------------------------*/

//#define forDebug 1
//#include "NewtonDebug.h"

#include "Objects.h"
#include "Refs.h"
#include "NewtonScript.h"
#include "Unicode.h"
#include "UStringUtils.h"


/*------------------------------------------------------------------------------
	F u n c t i o n   P r o t o t y p e s
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
	C o n s t a n t s
------------------------------------------------------------------------------*/

const unsigned kBufLen = 255;

typedef enum
{
	kNoToken,
	kLParenToken,
	kRParenToken,
	kLBracketToken,
	kRBracketToken,
	kStringToken,
	kNumberToken,
	kAtomToken,
	kLiteralToken,
	kEOLToken,
	kNilToken,
	kBadToken
} TokenType;

enum
{
	kNoState,
	kGenesisState,
	kTaggedState,
	kUnTaggedState,
	kUnTagged2State,
	kContinuationState,

	kOKState,
	kCAPABILITYState,
	kEXISTSState,
	kFETCHState,
	kFLAGSState,
	kUIDState,
	kENVELOPEState,
	kBODYState,
	kBYEState,

	kEnvDateState,
	kEnvSubjectState,
	kEnvFromState,
	kEnvSenderState,
	kEnvReplyToState,
	kEnvToState,
	kEnvCcState,
	kEnvBccState,
	kEnvAddressState,
	kEnvAddress1State,
	kEnvAddress2State,
	kEnvAddress3State,
	kEnvAddress4State,
	kEnvAddressDoneState,

	kSkipListState,

	kState
};


typedef struct
{
	UniChar *	name;
	int			state;
} KnownWord;

const KnownWord known[] =
{
	{ "OK", kOKState },
	{ "CAPABILITY", kCAPABILITYState },
	{ "EXISTS", kEXISTSState },
	{ "FETCH", kFETCHState },
	{ "FLAGS", kFLAGSState },
	{ "UID", kUIDState },
	{ "ENVELOPE", kENVELOPEState },
	{ "BODY", kBODYState },
	{ "BYE", kBYEState }
};

const unsigned kNumOfKnownWords = sizeof(known) / sizeof(KnownWord);


/*------------------------------------------------------------------------------
	U t i l i t i e s
------------------------------------------------------------------------------*/

Boolean IsSpace(UniChar ch)
{
	return ch == ' ' || ch == '\t';
}

Boolean IsDigit(UniChar ch)
{
	return ch >= '0' && ch <= '9';
}

UniChar * ToUpper(UniChar * str)
{
	UniChar * s = str, ch;
	while ((ch = *s++) != kEndOfUnicodeString)
		if (ch >= 'a' && ch <= 'z')
			ch -= ('a' - 'A');
	return str;
}


/*------------------------------------------------------------------------------
	T o k e n i z e r
------------------------------------------------------------------------------*/

typedef struct
{
	TokenType	type;
	long			number;
	UniChar		buf[kBufLen + 1];
} IMAPToken;


class IMAPTokenizer
{
public:
					IMAPTokenizer(RefArg inStr);
					~IMAPTokenizer();
	UniChar		GetC(void);
	void			UnGetC(void);
	void			AppendC(UniChar ch);
	void			NewToken(void);
	IMAPToken * Next(void);

	RefStruct	strVar;
	UniChar *	str;
	UniChar *	strPtr;
	IMAPToken	token;
	unsigned		bufLen;
};


IMAPTokenizer::IMAPTokenizer(RefArg inStr)
{
	strVar = inStr;
	str = (UniChar *) LockedBinaryPtr(strVar);
	NewToken();
}

IMAPTokenizer::~IMAPTokenizer(void)
{
	UnlockRefArg(strVar);
}

void IMAPTokenizer::NewToken(void)
{
	token.type = kNoToken;
	token.number = 0;
	token.buf[0] = kEndOfUnicodeString;
	bufLen = 0;
}

UniChar IMAPTokenizer::GetC(void)
{
	UniChar ch = *strPtr;
	if (ch != kEndOfUnicodeString)
		strPtr++;
	return ch;
}

void IMAPTokenizer::UnGetC(void)
{
	if (strPtr > str)
		strPtr--;
}

void IMAPTokenizer::AppendC(UniChar ch)
{
	if (bufLen < kBufLen)
		token.buf[bufLen++] = ch;
}


IMAPToken * IMAPTokenizer::Next(void)
{
	UniChar ch;

	if (token.type == kBadToken)
		return &token;

	NewToken();

	while (IsSpace(ch = GetC())
		;	// Skip leading whitespace

	switch (ch)
	{
	case '/n':
		token.type = kEOLToken;
		UnGetC();
		break;

	case '(':
		token.type = kLParenToken;
		break;

	case ')':
		token.type = kRParenToken;
		break;

	case '[':
		token.type = kLBracketToken;
		break;

	case ']':
		token.type = kRBracketToken;
		break;

	case '"':
		token.type = kStringToken;
		while ((ch = GetC()) != '"')
		{
			if (ch == '\\')
				ch = GetC();	// itÕs a quoted char
			if (ch == '\n')
			{
				UnGetC();
				token.type = kBadToken;	// end of input within string
				return &token;
			}
			AppendC(ch);
		}
		token.buf[bufLen] = kEndOfUnicodeString;
		break;

	case '{':
		token.type = kLiteralToken;
		while ((ch = GetC()) != '}')
		{
			if (IsDigit(ch))
				token.number = token.number * 10 + (ch - '0');
			else
			{
				UnGetC();
				token.type = kBadToken;	// end of input within literal
				return &token;
			}
		if ((ch = GetC()) != '\n')
			token.type = kBadToken;		// literal must be last item on line
		}
		break;

	default:
		if ((ch = GetC()) != '\\')		// itÕs a flag
		{
			AppendC(ch);
			ch = GetC();
		}
		else if (IsDigit(ch))			// itÕs a number
		{
			token.type = kNumberToken;
			token.number = ch - '0';
			AppendC(ch);
			for ( ; IsDigit(ch); ch = GetC())
			{
				token.number = token.number * 10 + (ch - '0');
				AppendC(ch);
			}
		}
		// Could be flag name or stuff like mime.spec, so continue reading.
		for ( ; ch != ' ' && ch != '\t' && ch != '\n'
			  && ch != '"' && ch != '\\'
			  && ch != '(' && ch != ')'
			  && ch != '{' && ch != '}'
			  && ch != '[' && ch != ']'; ch = GetC())
		{
			token.type = kAtomToken;
			AppendC(ch);
		}
		
		if (token.buf[0] == kEndOfUnicodeString)
			token.type = kBadToken;		// gotta have something!
		else
		{
			UnGetC();
			if (Ustrcmp(ToUpper(token.buf), "\000N\000I\000L\000") == 0) then
				token.type = kNilToken;
		}
	}

	return &token;
}


/*------------------------------------------------------------------------------
	P a r s e r
------------------------------------------------------------------------------*/

extern "C"
Ref ParseIMAPResponse(RefArg rcvr, RefArg inStr)
{
	RefVar imapState;
	IMAPTokenizer tokenizer(inStr);
	IMAPToken token;
	int tokenState, state;
	long seqNumber;

//	if (ISNIL(inState))
		tokenState = kGenesisState;
		
	while (tokenState != kNoState)
	{
		token = tokenizer.Next();
		state = tokenState;
		tokenState = kNoState;
		switch (state)
		{
		case kGenesisState:
			if (token.type == kAtomToken)
			{
				if (token.buf[0] == '*')
					tokenState = kUnTaggedState;
				else if (Ustrcmp(token.buf, cmdTag) == 0)
					tokenState = kTaggedState;
				else if (token.buf[0] == '+') then
					tokenState = kContinuationState;
			}
			break;

		case kTaggedState:
			if (Ustrcmp(token.buf, "\000O\000K\000") then
				//	OK response
				imapState = SYM(ok);
			else
				//	NO or BAD response
				imapState = SYM(err);

		case kUnTaggedState:
			if (token.type == kNumberToken)
			{
				seqNumber = token.num;
				tokenState = kUnTagged2State;
				break;
			}
			// else fall thru

		case kUnTagged2State:
			if (token.type == kAtomToken)
			{
				for (int i = 0; i < kNumOfKnownWords; i++)
				{
					if (Ustrcmp(known[i].name, token.buf) == 0)
					{
						tokenState = known[i].state;
						break;
					}
				}
			}
			break;

		case kContinuationState:
			imapState = MakeString(token.buf);
			break;

	// ¥¥ OK
		case kOKState:
		// Untagged OK response.
		// No need to do anything - this is the "* OK IMAP server ready" greeting.
			imapState = SYM(ok);
			break;

	// ¥¥ CAPABILLITY
		case kCAPABILITYState:
/*
local start := 0, stop;
while start := StrPos(data, "AUTH=", start) do
	begin
	start := start + 5;
	stop := CharPos(data, $ , start);
	local mech := SubStr(data, start, if stop then stop - start else StrLen(data) - 1 - start);
	if StrEqual(mech, "CRAM-MD5") then
		break authMechanism := 'cram;
	end;
nil
*/
			break;

	// ¥¥ EXISTS
		case kEXISTSState:
			SetVariable(rcvr, SYM(msgCount), MAKEINT(seqNumber));
			break;

	// ¥¥ FETCH
		case kFETCHState:
			if (token.type == kLParenToken)	// ignore (
				tokenState = kFETCHState;
			else if (token.type == kAtomToken)
				tokenState = huh;
			else if (token.type == kLiteralToken)
				tokenState = huh;
			break;

	// ¥¥ FLAGS
		case kFLAGState:
			if (token.type == kRParenToken)
				tokenState = kFETCHState;
			else if (token.type != kEOLToken)
				tokenState = kFLAGState;	// ignore them for now
			break;

	// ¥¥ UID
		case kUIDState:
			if (token.type == kNumberToken)
				SetFrameSlot(msg, SYM(id), MakeString(token.buf));
			tokenState = kFETCHState;
			break;

	// ¥¥ ENVELOPE
		case kENVELOPEState:
		// ENVELOPE structure is:
		// ( date subject from sender reply-to to cc bcc in-reply-to message-id )
			if (token.type == kLParenToken)	// must have (
				tokenState = kEnvDateState;
			break;

		case kEnvDateState:
			msg.timestamp = :InternetTimestamp(token.buf);
			tokenState = kEnvSubjectState;
			break;

		case kEnvSubjectState:
			msg.title = call SM.DecodeHeader with (token.buf);
			tokenState = kEnvFromState;
			break;

		case kEnvFromState:
			if (token.type == kLParenToken)
			{
				addressRef = SYM(fromRef);
				addressNextState = kEnvSenderState;
				tokenState = kEnvAddressState;
			}
			else
				tokenState = kEnvSenderState;
			break;

		case kEnvSenderState:
			if (token.type == kLParenToken)
			{
				addressRef = NILREF;	// donÕt actually use the sender
				addressNextState = kEnvReplyToState;
				tokenState = kEnvAddressState;
			}
			else
				tokenState = kEnvReplyToState;
			break;

		case kEnvReplyToState:
			if (token.type == kLParenToken)
			{
				addressRef = SYM(reply-to);
				addressNextState = kEnvToState;
				tokenState = kEnvAddressState;
			}
			else
				tokenState = kEnvToState;
			break;

		case kEnvToState:
			if (token.type == kLParenToken)
			{
				addressRef = SYM(toRef);
				addressNextState = kEnvCcState;
				tokenState = kEnvAddressState;
			}
			else
				tokenState = kEnvCcState;
			break;

		case kEnvCcState:
			if (token.type == kLParenToken)
			{
				addressRef = SYM(cc);
				addressNextState = kEnvBccState;
				tokenState = kEnvAddressState;
			}
			else
				tokenState = kEnvBccState;
			break;

		case kEnvBccState:
			if (token.type == kLParenToken)
			{
				addressRef = SYM(bcc);
				addressNextState = kSkipListState;
				tokenState = kEnvAddressState;
			}
			else
				tokenState = kSkipListState;	// ignore remaining items
			break;

		case kEnvAddressState:
			if (token.type == kLParenToken)
			{
				addressParts = MakeArray(4);
				tokenState = kEnvAddress1State;
			}
			else
				tokenState = addressNextState;
			break;

		case kEnvAddress1State:
		// First item in address is the name.
			if (token.type == kStringToken)
				addressParts[0] = call SM.DecodeHeader with (token.buf);
			tokenState = kEnvAddress2State;
			break;

		case kEnvAddress2State:
		// Second item in address is the source route.
		// Ignore it.
			if (token.type == kLParenToken)
				while ((token = tokenizer.Next()) != kRParenToken)		// POSSIBLE INFINITE LOOP
					;
			tokenState = kEnvAddress3State;
			break;

		case kEnvAddress3State:
		// Third item in address is the mailbox name.
			if (token.type == kStringToken)
				addressParts[2] = token.buf;
			tokenState = kEnvAddress4State;
			break;

		case kEnvAddress4State:
		// Fourth item in address is the host name.
			if (token.type == kStringToken)
				addressParts[3] = token.buf;
			tokenState = kEnvAddressDoneState;
			break;

		case kEnvAddressDoneState:
			if (token.type == kRParenToken)	// must have )
			{
				if (NOTNIL(addressRef))
				{
					local nameRef = :MakeMailNameRef(addressParts[0], addressParts[2], addressParts[3]);
					if nameRef then
						begin
						if IsArray(msg.(addressRef)) then
							AddArraySlot(msg.(addressRef), nameRef)
						else if msg.(addressRef) = nil then	// possibly multiple addressees - choose the first
							begin
							msg.(addressRef) = nameRef;
							if addressRef = 'fromRef' then
								msg.fromName =	if nameRef.company then
															Clone(nameRef.company)
														else if nameRef.name then
															TrimString(nameRef.name.first && nameRef.name.last);
							end
						end
				}
				tokenState = kEnvAddressState;	// look for another item
			}
			else if (token.type != kEOLToken)
				tokenState = kEnvAddressDoneState;		// keep looking for the terminator
			break;
	
		case kSkipListState:
			if (token.type == kRParenToken)	// wait for )
				tokenState = kFETCHState;
			else
				tokenState = kSkipListState;
			break;

	// ¥¥ BODY
		case kBODYState:
		// This could be the BODY structure
		// or the BODY[] part
			if (token.type == kLBracketToken)
				tokenState = kBodyPartState;
			else if (token.type == kLParenToken)
				tokenState = kBodyStructState;
			break;

		case kBodyPartState:
			break;

		case kBodyStructState:
		// BODY structure is:
		// ( type subtype parms id description )
constant kStructFrame = '{ type:text, subtype:plain, parms:nil, description:nil };

if token.type = 'lParen then	// itÕs a multipart message
	begin
	msg.struct = [kStructFrame];
	|body-type|
	end
else
	begin
	msg.struct = kStructFrame;
	:|body-type|(token)
	end
			break;

		case kBodyTypeState:
			tokenState = kBodySubtypeState;
			break;

		case kBodySubtypeState:
			tokenState = kBodyParmsState;
			break;

		case kBodyParmsState:
			tokenState = kBodyIdState;
			break;

		case kBodyIdState:
			tokenState = kBodyDescriptionState;
			break;

		case kBodyDescriptionState:
			tokenState = kBodyDoneState;
			break;

		case kBodyDoneState:
			if (token.type == kRParenToken)	// wait for )
				tokenState = kFETCHState;		// ?
			else
				tokenState = kBodyDoneState;
			break;

	// ¥¥ BYE
		case kBYEState:
			SetVariable(rcvr, SYM(fsmState), SYM(LogoutDone));
			imapState = SYM(ok);
			break;

		}
	}
	return imapState;
}

