
#include "../../proj/tl_common.h"
#include "aes_ccm.h"


#if !defined(WIN32)
#define SOFT_AES	1
#else
#define SOFT_AES	0
#endif
/*
typedef struct {
	u8		iat;			//initiator address type
	u8		rat;			//response address type
	u8		preq[7];		//request
	u8		pres[7];		//response
} smp_p1_t;

typedef struct {
	u8		ra[6];			//initiator address type
	u8		ia[6];			//response address type
	u8		padding[4];		//request
} smp_p2_t;

  p1 = pres || preq || rat|| iat
    For example, if the 8-bit  iat  is 0x01, the 8-bit  rat  is 0x00, the 56-bit  preq is
    0x07071000000101 and the 56 bit pres  is 0x05000800000302  then p1 is
    0x05000800000302070710000001010001.
    c1 (k, r, preq, pres, iat, rat, ia , ra) = e(k, e(k,  r XOR p1) XOR p2)
*/

static void aes_ll_swap (u8 *k)
{
	for (int i=0; i<8; i++)
	{
		u8 t = k[i];
		k[i] = k[15 - i];
		k[15 - i] = t;
	}
}

void aes_ll_c1(u8 * key, u8 * r, u8 *p1, u8 *p2, u8 * result)
{
	u8 p[16];
	int i;
	for (i=0; i<16; i++){
		p[i] = r[i] ^ p1[i];
	}
	aes_ll_encryption (key, p, result);		//output MSB first
	for (i=0; i<16; i++){
		p[i] = result[15-i] ^ p2[i];
	}
	aes_ll_encryption (key, p, result);
	aes_ll_swap (result);
}

void aes_ll_s1(u8 * key, u8 * r1, u8 * r2, u8 * result)
{
	memcpy (result, r1, 8);
	memcpy (result+8, r2, 8);
	aes_ll_encryption (key, result, result);
	aes_ll_swap (result);
}


#ifdef WIN32
#include "../../vendor/fastAES/src/EfAes.H"
AesCtx context;
static inline u8 hwAes_encrypt(u8 *key, u8 *data, u8 *result){
	if(key){
		AesSetKey( &context , AES_KEY_128BIT ,BLOCKMODE_ECB, key , 0 );
	}
	AesEncryptECB(&context ,result, data, 16);
	return 0;
}
#elif SOFT_AES
static inline u8 hwAes_encrypt(u8 *key, u8 *data, u8 *result){
	memcpy(result, data, 16);
	_rijndaelEncrypt (result);
    return 0;
}
#else
static inline u8 hwAes_encrypt(u8 *key, u8 *data, u8 *result){    
	u8 r = irq_disable();
    while ( reg_aes_ctrl & BIT(1) ) {
    	reg_aes_data = (data[0]) | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
        data += 4;
    }

    /* start encrypt */
    reg_aes_ctrl = 0x00;
    
    /* wait for aes ready */
    while ( !(reg_aes_ctrl & BIT(2)) );

    /* read out the result */
    for (u8 i=0; i<4; i++) {
		u32 temp = reg_aes_data;
		result[0] = temp; result[1] = temp >> 8; result[2] = temp >> 16;  result[3] = temp >> 24;
		result += 4;
    }
	irq_restore(r);
    return 0;
}
#endif

static inline u8 aes_encrypt(u8 *key, u8 *data, u8 *result){
    return hwAes_encrypt(key, data, result);
}

u8 aes_initKey(u8 *key){
#ifdef WIN32
	AesSetKey( &context , AES_KEY_128BIT ,BLOCKMODE_ECB, key , 0 );
#else
    u16 aesKeyStart = 0x550;
    for (u8 i=0; i<16; i++) {
     REG_ADDR8(aesKeyStart + i) = key[i];
    }
#endif
	return AES_SUCC;
}

#if __DEBUG_AES__
u8 my_micIndex=0;
u8 my_micValue[100];

u8 my_aes[160];
u8 my_aesIndex = 0;
#endif


/*********************************************************************
 * @fn      aes_ccmAuthTran
 *
 * @brief   calc the aes ccm value 
 *
 * @param   micLen - mic lenth (should be 4)
 *
 * @param   iv - initial vector (should be 13 bytes nonce)
 *
 * @param   mStr - plaint text 
 *
 * @param   mStrLen - plaint text length
 *
 * @param   aStr -  a string  (should be AAD the data channel PDU header’s first octet with NESN, SN and MD bits masked to 0)
 *
 * @param   aStrLen - a atring lenth (should be 1)
 *
 * @param   result - result (result)
 *
 * @return  status l2cap_sts_t
 */
_attribute_ram_code_ u8 aes_ccmAuthTran(u8 *iv, u8 *mStr, u16 mStrLen, u8 aStr, u8 *result)
{
	u8 aStrLen;
    u16 msgLen;
    u8 mStrIndex = 0;
    u16 i,j;

    //aes_enc_t *tmpPtr = (aes_enc_t*)ev_buf_allocate(sizeof(aes_enc_t));
    aes_enc_t *tmpPtr, encTmp;
    tmpPtr = &encTmp;
    memset(tmpPtr, 0, sizeof(aes_enc_t));

    tmpPtr->bf.B[0] = 0b01001001;
    /* copy nonce N */
    memcpy(tmpPtr->bf.B + 1, iv, 13);
    /* last byte is mStrlen */
    tmpPtr->bf.B[14] = mStrLen>>8;
    tmpPtr->bf.B[15] = mStrLen & 0xff;

    tmpPtr->newAstr[0] = 1>>8;
    tmpPtr->newAstr[1] = 1 & 0xff;
    tmpPtr->newAstr[2] = aStr;
    aStrLen = 3;

    /* X0 is zero */
    memset(tmpPtr->tmpResult, 0, AES_BLOCK_SIZE);

    msgLen = 3;
    /* adjust msgLen according to aStrLen and mStrLen, should be 16x */
    if (3 & 0x0f) {
        msgLen &= ~0x0F;
        msgLen += 0x10;
    }

    msgLen += mStrLen;
    if (mStrLen & 0x0f) {
        msgLen &= ~0x0F;
        msgLen += 0x10;
    }

    
    /* now the msgLen should be the length of AuthData, which is generated by AddAuthData (astring, padded by 0) || PlaintexeData (mString, padded by 0)*/
    for ( i=0; i<msgLen+16; i+=(1<<4) ) {
        for ( j=0; j<AES_BLOCK_SIZE; j++) {
            /* get Xi XOR Bi */
            tmpPtr->tmpResult[j] ^= tmpPtr->bf.B[j];
        }

        /* use aes to get E(key, Xi XOR Bi) */
        aes_encrypt(0, tmpPtr->tmpResult, tmpPtr->tmpResult);
        /* update B */
		if ( (aStrLen>0) && (aStrLen<AES_BLOCK_SIZE) ) {
            memcpy(tmpPtr->bf.B, tmpPtr->newAstr + i, aStrLen);
            memset(tmpPtr->bf.B + aStrLen, 0, AES_BLOCK_SIZE - aStrLen);
            aStrLen = 0;
            /* reset the mstring index */
            mStrIndex = 0;
        } else if ( mStrLen >= AES_BLOCK_SIZE ) {
            memcpy(tmpPtr->bf.B, mStr + (mStrIndex*AES_BLOCK_SIZE), AES_BLOCK_SIZE);
            mStrIndex++;
            mStrLen -= AES_BLOCK_SIZE;
        } else {
            memcpy(tmpPtr->bf.B, mStr + (mStrIndex*AES_BLOCK_SIZE), mStrLen);
            memset(tmpPtr->bf.B + mStrLen, 0, AES_BLOCK_SIZE - mStrLen);
        }
    }
    memcpy(result, tmpPtr->tmpResult, 4);
    
    return 0;
}

/*********************************************************************
 * @fn      aes_ccmBaseTran
 *
 * @brief   calc the aes ccm value 
 *
 * @param   micLen - mic lenth (should be 4)
 *
 * @param   iv - initial vector (should be 13 bytes nonce)
 *
 * @param   mStr - plaint text 
 *
 * @param   mStrLen - plaint text length
 *
 * @param   aStr -  a string  (should be AAD the data channel PDU header’s first octet with NESN, SN and MD bits masked to 0)
 *
 * @param   aStrLen - a atring lenth (should be 1)
 *
 * @param   result - result (result)
 *
 * @return  status l2cap_sts_t
 */
_attribute_ram_code_ u8 aes_ccmBaseTran(u8 *iv, u8 *mStr, u16 mStrLen, u8 aStr, u8 *mic, u8 opt)
{
    u8 msgLen;
    u16 i;
    u8 j;
    u16 counter = 1;

    //aes_enc_t *tmpPtr = (aes_enc_t*)ev_buf_allocate(sizeof(aes_enc_t));
    aes_enc_t *tmpPtr, encTmp;
    tmpPtr = &encTmp;
    memset(tmpPtr, 0, sizeof(aes_enc_t));
    tmpPtr->bf.A[0] = 1;

    /* set the iv */
    memcpy(tmpPtr->bf.A+1, iv, 13);

    tmpPtr->bf.A[14] = tmpPtr->bf.A[15] = 0;

    aes_encrypt(0, tmpPtr->bf.A, tmpPtr->tmpResult);

    for ( i=0; i<4; i++ ) {
        mic[i] ^= tmpPtr->tmpResult[i];
    }
    
    tmpPtr->bf.A[14] = counter>>8;
    tmpPtr->bf.A[15] = counter & 0xff;

    if ( opt == AES_DECRYPTION ) {
        msgLen = mStrLen - 4;
    }
    
    msgLen = mStrLen;
    if (msgLen & 0x0f) {
        msgLen &= ~0x0F;
        msgLen += 0x10;
    }


    for ( i=0; i<msgLen; i+=(1<<4) ) {
        /* use aes to the E(key, Ai) */
        aes_encrypt(0, tmpPtr->bf.A, tmpPtr->tmpResult);
        //tmpResult = TODO();

        for ( j=0; (j<AES_BLOCK_SIZE) && (i+j < mStrLen); j++) {
            mStr[i+j] ^= tmpPtr->tmpResult[j];
        }

        /* update Ai */
        counter++;
        tmpPtr->bf.A[14] = counter>>8;
        tmpPtr->bf.A[15] = counter & 0xff;
    }
    
#if 0	
#if (__DEBUG_BUFM__)
    if ( SUCCESS != ev_buf_free((u8*)tmpPtr) ) {
		while(1);
    }
#else 
    ev_buf_free((u8*)tmpPtr);
#endif
#endif
    return 0;
}

static inline u8 aes_ccmEncTran(u8 *iv, u8 *mStr, u16 mStrLen, u8 aStr, u8 *mic)
{
	return aes_ccmBaseTran(iv, mStr, mStrLen, aStr, mic, AES_ENCRYPTION);
}

static inline u8 aes_ccmDecTran(u8 *iv, u8 *mStr, u16 mStrLen, u8 aStr, u8 *mic)
{
    return aes_ccmBaseTran(iv, mStr, mStrLen, aStr, mic, AES_DECRYPTION);
}

static inline u8 aes_ccmDecAuthTran(u8 *iv, u8 *mStr, u16 mStrLen, u8 aStr, u8 *mic)
{
    u8 tmpMic[4];
    u8 i;
    aes_ccmAuthTran(iv, mStr, mStrLen, aStr, tmpMic);
    for ( i=0; i<4; i++ ) {
        if ( mic[i] != tmpMic[i] ) {
            return -1;
        }
    }
    return 0;
}

#ifdef WIN32
#include "../../vendor/fastAES/src/EfAes.H"
u8 aes_ll_encryption(u8 *key, u8 *plaintext, u8 *result){
	u8 keytemp[16];
	u8 texttemp[16];
	swap128(keytemp, key); swap128(texttemp, plaintext);
	AesCtx context;
	AesSetKey( &context , AES_KEY_128BIT ,BLOCKMODE_ECB, keytemp , 0 );
	AesEncryptECB(&context ,result, texttemp, 16);
	return 0;
}
#elif SOFT_AES
u8 aes_ll_encryption(u8 *key, u8 *plaintext, u8 *result){
	u8 sk[16];
	int i;
	for (i=0; i<16; i++)
	{
		sk[i] = key[15 - i];
	}
	_rijndaelSetKey (sk);

	for (i=0; i<16; i++)
	{
		sk[i] = plaintext[15 - i];
	}
	_rijndaelEncrypt (sk);

	 memcpy (result, sk, 16);
}
#else
u8 aes_ll_encryption(u8 *key, u8 *data, u8 *result)
{
	u8 r = irq_disable();
	int i;

	u16 aesKeyStart = 0x550;
	for (i=0; i<16; i++) {
		REG_ADDR8(aesKeyStart + i) = key[15 - i];
	}

    data += 12;
    /* feed the data */
    for (int i=0; i<4; i++)
    {
    	reg_aes_data = (data[3]) | (data[2]<<8) | (data[1]<<16) | (data[0]<<24);
    	data -= 4;
    }

    /* start encrypt */
    reg_aes_ctrl = 0x00;

    /* wait for aes ready */
    while ( !(reg_aes_ctrl & BIT(2)) );

    /* read out the result */
    for (i=0; i<4; i++) {
		u32 temp = reg_aes_data;
		result[0] = temp; result[1] = temp >> 8; result[2] = temp >> 16;  result[3] = temp >> 24;
		result += 4;
    }
	irq_restore(r);
	return AES_SUCC;
}
#endif

typedef struct {
	u32		pkt;
	u8		dir;
	u8		iv[8];
} ll_enc_nonce_t;

ll_enc_nonce_t ll_enc_nonce = {0};
u32	ll_enc_pno;
u32	ll_dec_pno;

void aes_ll_ccm_encryption_init (u8 *ltk, u8 *skdm, u8 *skds, u8 *ivm, u8 *ivs){
	u8 sk[16];
	memcpy (sk, skdm, 8);
	memcpy (sk+8, skds, 8);
#if SOFT_AES
	u8 i;
	for (i=0; i<16; i++){
		sk[i] = ltk[15 - i];
	}
	_rijndaelSetKey(sk);
	for (i=0; i<8; i++){
		sk[i] = skds[7 - i];
		sk[i+8] = skdm[7 - i];
	}
	_rijndaelEncrypt(sk);
	_rijndaelSetKey(sk);
#else
	aes_ll_encryption (ltk, sk, sk);
	aes_initKey(sk);
#endif	
	memcpy (ll_enc_nonce.iv, ivm, 4);
	memcpy (ll_enc_nonce.iv + 4, ivs, 4);
	ll_enc_pno = 0;
	ll_dec_pno = 0;
}

void aes_ll_ccm_encryption(u8 *pkt, int master)
{
	u8 mic[4];
	u8 llid = pkt[0] & 3;
	u8 len = pkt[1];
	pkt[1] += 4;
	pkt += 2;
	ll_enc_nonce.pkt = ll_enc_pno++;
	ll_enc_nonce.dir = master ? 0x80 : 0;
	aes_ccmAuthTran((u8*)&ll_enc_nonce, pkt, len, llid, mic);
    aes_ccmEncTran((u8*)&ll_enc_nonce, pkt, len, llid, mic);
    memcpy (pkt + len, mic, 4);
}

int aes_ll_ccm_decryption(u8 *pkt, int master)
{
	pkt[1] -= 4;
	u8 llid = pkt[0] & 3;
	u8 len = pkt[1];
	pkt += 2;
	ll_enc_nonce.pkt = ll_dec_pno++;
	ll_enc_nonce.dir = master ? 0x80 : 0;
	aes_ccmDecTran((u8*)&ll_enc_nonce, pkt, len, llid, pkt + len);
    u8 r = aes_ccmDecAuthTran((u8*)&ll_enc_nonce, pkt, len, llid, pkt + len);
    return r;
}

