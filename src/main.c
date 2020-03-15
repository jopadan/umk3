/*
 *          Movie Sample Program
 *
 *      Copyright (C) 1993 by Sony Corporation
 *          All rights Reserved
 *
 *   Version    Date
 *  ------------------------------------------
 *  1.00        Jul,14,1994 yutaka
 *  1.10        Sep,01,1994 suzu
 *  1.20        Oct,24,1994 yutaka
 *                  (anim sub routine��)
 *  1.30        Nov,01,1994 ume
 *                  (�`��I�t�Z�b�g�ݒ�A
 *                  �T�C�Y���P�U�̔{���ȊO�̃C���[�W�ɑΉ�)
 *
 *****************RELEASE NOTE****************************
 * \psx\sample\movie\anim������main.c�Œu�������ĉ������B
 * makefile.mak��sim.o�������N���Ă���̂���߂ĉ������B
 * �f�B�X�N��DTL-S2160���g�p���ĉ������B
 * �Đ��t���[���̒�����FRAME_SIZE�ł͂Ȃ�END_FRAME�ł���ĉ�����
 * FRAME_SIZE�́A0xfffffff�����ĉ������Bend_callback�͂O��
 * �w�肵�ĉ������B
 *
 */

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libcd.h>
#include <r3000.h>
#include <asm.h>
#include <kernel.h>
#include <libsn.h>

/*#define EMULATE*/

#ifdef EMULATE
#define StGetNext   StGetNextS
#define StFreeRing  StFreeRingS
#endif

#define IS_RGB24    1   /* 0:RGB16, 1:RGB24 */

#if IS_RGB24==1
#define PPW 3/2         /* �P�V���[�g���[�h�ɉ��s�N�Z�����邩 */
#define MODE    3       /* 24bit ���[�h�Ńf�R�[�h */
#else
#define PPW 1           /* �P�V���[�g���[�h�ɉ��s�N�Z�����邩 */
#define MODE    2       /* 16bit ���[�h�Ńf�R�[�h */
#endif

#define END_FRAME     260   /* �A�j���[�V�������I��点��t���[�� */
static CdlLOC loc;                      /* CDROM���Đ�������|�C���g */

#define FILENAME    "\\MOVIE.STR;1"       /* �t�@�C���l�[�� */
#define OFFY 0                          /* �`��I�t�Z�b�g(Y ���W) */
#define OFFX 0                          /* �`��I�t�Z�b�g(X ���W�A����) */

#define bound(val, n)   ((((val) - 1) / (n) + 1) * (n))
#define bound16PPW(val)   (bound((val),16*PPW))
#define bound16(val)   (bound((val),16))

static int isFirstSlice;                /* �ō��[�X���C�X�������t���O */

/*
 *  �f�R�[�h��
 */
typedef struct {
    u_long  *vlcbuf[2]; /* VLC �o�b�t�@�i�_�u���o�b�t�@�j */
    int vlcid;      /* ���� VLC �f�R�[�h���o�b�t�@�� ID */
    u_short *imgbuf;    /* �f�R�[�h�摜�o�b�t�@�i�V���O���j*/
    RECT    rect[2];    /* �]���G���A�i�_�u���o�b�t�@�j */
    int rectid;     /* ���ݓ]�����̃o�b�t�@ ID */
    RECT    slice;      /* �P��� DecDCTout �Ŏ��o���̈� */
    int isdone;     /* �P�t���[�����̃f�[�^���ł����� */
} DECENV;

static DECENV   dec;        /* �f�R�[�h���̎��� */
static int      Rewind_Switch;  /* CD���I��܂ł����ƂP�ɂȂ� */
static int      Clear_Flag; /* ��ʂ̉𑜓x���ς���ʂ̃N���A��
                   �K�v�Ȏ��ɂP�ɂȂ� */

CdlFILE     fp;     /* �t�@�C���̈ʒu�E�T�C�Y��� */

/*
 * �R�[���o�b�N�֐��^
 */
typedef int (*CallbackFunc)();

/*
 * static �֐��錾
 */
static strInit(CdlLOC *loc, int frame_size,
    CallbackFunc callback, CallbackFunc endcallback);
static int strCallback();
static strNextVlc(DECENV *dec);
static u_long *strNext(DECENV *dec);
static strSync(DECENV *dec, int mode);
static strKickCD(CdlLOC *loc);
static strSetDefDecEnv(DECENV *dec, int x0, int y0, int x1, int y1);
static void anim(char *fileName);

main()
{
	struct EXEC exec;

    /* �A�j���[�V���� ����ȊO ���ʂ̏����� */
    ResetCallback();
    CdInit();
    PadInit(0);     /* PAD �����Z�b�g */
    ResetGraph(0);      /* GPU �����Z�b�g */
    SetGraphDebug(0);   /* �f�o�b�O���x���ݒ� */

    /* �t�H���g�����[�h */
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(32, 32, 320, 200, 0, 512));

    anim(FILENAME);         /* �A�j���[�V�����T�u���[�`�� */

	PadStop();
	StopCallback();
	ResetGraph(3);

	_96_remove();
	_96_init();
	LoadExec("cdrom:\\MK3.EXE;1", 0x801fff00, 0);

	return(0);
}

/*
 *  �A�j���[�V�����T�u���[�`�� �t�H�A�O���E���h�v���Z�X
 */
static void anim(char *fileName)
{
    DISPENV disp;
    DRAWENV draw;

    int frame = 0;      /* �t���[���J�E���^ */
    int id;         /* �\���o�b�t�@ ID */
    volatile u_long  enc_count; /* �J�E���^ */

    u_char  result[8], ret;     /* CD-ROM status */
    CdlLOC  ppos, lpos;     /* CD-ROM position */
    CdlFILE file;
    int fn;
    u_char  param[8];

    isFirstSlice = 1;

    /* �t�@�C�����T�[�` */
    if (CdSearchFile(&file, fileName) == 0) {
        StopCallback();
        PadStop();
        exit();
    }

    loc.minute = file.pos.minute;
    loc.second = file.pos.second;
    loc.sector = file.pos.sector;

    /* ������ */
    strSetDefDecEnv(&dec, OFFX*PPW, OFFY, OFFX*PPW, 240 + OFFY);

    strInit(&loc, 0xfffffff, strCallback, 0);

    /* ���̃t���[���̃f�[�^���Ƃ��Ă��� VLC �̃f�R�[�h���s�Ȃ� */
    /* ���ʂ́Adec.vlcbuf[dec.vlcid] �Ɋi�[�����B*/
    strNextVlc(&dec);

    while (1) {
        /* VLC �̊��������f�[�^�𑗐M */
        DecDCTin(dec.vlcbuf[dec.vlcid], MODE);

        /* �ŏ��̃f�R�[�h�u���b�N�̎�M�̏��������� */
        DecDCTout(dec.imgbuf,
            bound16PPW(dec.slice.w)*bound16(dec.slice.h)/2);

        /* ���̃t���[���̃f�[�^�� VLC �f�R�[�h */
        strNextVlc(&dec);

        /* �f�[�^���o���オ��̂�҂� */
        strSync(&dec, 0);

        /* V-BLNK ��҂� */
        VSync(0);

        /* �\���o�b�t�@���X���b�v */
        /* �\���o�b�t�@�́A�`��o�b�t�@�̔��Α��Ȃ��Ƃɒ��� */
        id = dec.rectid? 0: 1;
        SetDefDispEnv(&disp, dec.rect[id].x - OFFX*PPW,
            dec.rect[id].y - OFFY,
            dec.rect[id].w, dec.rect[id].h);
        SetDefDrawEnv(&draw, dec.rect[id].x, dec.rect[id].y,
            dec.rect[id].w, dec.rect[id].h);

#if IS_RGB24==1
        disp.isrgb24 = IS_RGB24;
        disp.disp.w = disp.disp.w*2/3;
#endif
        PutDispEnv(&disp);
        PutDrawEnv(&draw);
        SetDispMask(1);       /* �\������ */

#if IS_RGB24==0
        FntFlush(-1);
#endif

        if(Rewind_Switch == 1)
        break;

        if(PadRead(1) & PADk)
            /* �X�g�b�v�{�^���������ꂽ��A�j���[�V�����𔲂��� */
            break;
    }

    /* �A�j���[�V�����㏈�� */
    param[0] = CdlModeSpeed;
    CdControlB(CdlSetmode,param,0);
    DecDCToutCallback(0);
    CdDataCallback(0);
    CdReadyCallback(0);
    CdControlB(CdlPause,0,0);
}

/*
 * �f�R�[�h����������
 * imgbuf, vlcbuf �͍œK�ȃT�C�Y�����蓖�Ă�ׂ��ł��B
 */
static strSetDefDecEnv(DECENV *dec, int x0, int y0, int x1, int y1)
{
    static u_long   vlcbuf0[320/2*256];     /* �傫���K�� */
    static u_long   vlcbuf1[320/2*256];     /* �傫���K�� */
    static u_short  imgbuf[16*PPW*240*2];       /* �Z��P�� */

    dec->vlcbuf[0] = vlcbuf0;
    dec->vlcbuf[1] = vlcbuf1;
    dec->vlcid     = 0;
    dec->imgbuf    = imgbuf;
    dec->rectid    = 0;
    dec->isdone    = 0;
    setRECT(&dec->rect[0], x0, y0, 640*PPW, 240);
    setRECT(&dec->rect[1], x1, y1, 640*PPW, 240);
    setRECT(&dec->slice,   x0, y0,  16*PPW, 240);
}

/*
 * �X�g���[�~���O����������
 */
#define RING_SIZE   32      /* �����O�o�b�t�@�T�C�Y */

static u_long   sect_buff[RING_SIZE*SECTOR_SIZE];/* �����O�o�b�t�@ */

static u_long   sect_buff0[RING_SIZE*SECTOR_SIZE];/* �����O�o�b�t�@ */

static strInit(CdlLOC *loc, int frame_size,
    CallbackFunc callback, CallbackFunc endcallback)
{
    DecDCTReset(0);     /* MDEC �����Z�b�g */
    Rewind_Switch = 0;  /* �����߂��O */

    /* MDEC���P�f�R�[�h�u���b�N�������������̃R�[���o�b�N���`���� */
    DecDCToutCallback(callback);

    /* �����O�o�b�t�@�̐ݒ� */
    StSetRing(sect_buff, RING_SIZE);

    /* �X�g���[�~���O���Z�b�g�A�b�v */
    /* �Ō�܂ł������� endcallback() ���R�[���o�b�N����� */
    StSetStream(IS_RGB24, 1, frame_size, 0, endcallback);

    /* �ŏ��̂P�t���[�����Ƃ��Ă��� */
    strKickCD(loc);
}


/*
 * �o�b�N�O���E���h�v���Z�X
 * (DecDCTout() ���I�������ɌĂ΂��R�[���o�b�N�֐�)
 */
static int strCallback()
{
    int mod;

#if IS_RGB24==1
    extern StCdIntrFlag;
    if(StCdIntrFlag) {
        StCdInterrupt();    /* RGB24�̎��͂����ŋN������ */
        StCdIntrFlag = 0;
    }
#endif

    /* �f�R�[�h���ʂ��t���[���o�b�t�@�ɓ]�� */
    LoadImage(&dec.slice, (u_long *)dec.imgbuf);

    /* �Z���`�̈���ЂƂE�ɍX�V */
    if (isFirstSlice && (mod = dec.rect[dec.rectid].w % (16*PPW))) {
        dec.slice.x += mod*PPW;
        isFirstSlice = 0;
    }
    else {
        dec.slice.x += 16*PPW;
    }

    /* �܂�����Ȃ���΁A*/
    if (dec.slice.x < dec.rect[dec.rectid].x + dec.rect[dec.rectid].w) {
        /* ���̒Z�����M */
        DecDCTout(dec.imgbuf, bound16PPW(dec.slice.w)*bound16(dec.slice.h)/2);
    }
    /* �P�t���[�����I������A*/
    else {
        /* �I�������Ƃ�ʒm */
        dec.isdone = 1;
        isFirstSlice = 1;

        /* ID ���X�V */
        dec.rectid = dec.rectid? 0: 1;
        dec.slice.x = dec.rect[dec.rectid].x;
        dec.slice.y = dec.rect[dec.rectid].y;

        /*DecDCTout(dec.imgbuf, dec.slice.w*dec.slice.h/2);*/
    }
}


/* ���̃t���[���̃f�[�^����݂��� VLC �̉𓀂��s�Ȃ�
 * �G���[�Ȃ�΃����O�o�b�t�@���N���A���Ď��̃t���[�����l������
 * VLC���f�R�[�h���ꂽ�烊���O�o�b�t�@�̂��̃t���[���̗̈��
 * �K�v�Ȃ��̂Ń����O�o�b�t�@�̃t���[���̗̈���������
 */
static strNextVlc(DECENV *dec)
{
    int cnt = WAIT_TIME;
    u_long  *next, *strNext();

    while ((next = strNext(dec)) == 0) {        /* get next frame */
        if (--cnt == 0)
            return(-1);
    }

    dec->vlcid = dec->vlcid? 0: 1;          /* swap ID */
    DecDCTvlc(next, dec->vlcbuf[dec->vlcid]);   /* VLC decode */
    StFreeRing(next);               /* free used frame */
    return(0);
}


/*
 * ���̂P�t���[����MOVIE�t�H�[�}�b�g�̃f�[�^�������O�o�b�t�@����Ƃ��Ă���
 * �f�[�^�����킾������ �����O�o�b�t�@�̐擪�A�h���X��
 * ����łȂ���� NULL��Ԃ�
 */
typedef struct {
    u_short id;         /* always 0x0x0160 */
    u_short type;
    u_short secCount;
    u_short nSectors;
    u_long  frameCount;
    u_long  frameSize;

    u_short width;
    u_short height;

    u_long  headm;
    u_long  headv;
} CDSECTOR;             /* CD-ROM STR �\���� */

static u_long *strNext(DECENV *dec)
{
    u_long      *addr;
    CDSECTOR    *sector;
    int     cnt = WAIT_TIME;
    static  int      width  = 0;            /* ��ʂ̉��Əc */
    static  int      height = 0;

    while(StGetNext(&addr,(u_long**)&sector)) {
        if (--cnt == 0)
          return(0);
    }

    /* �Z�N�^�w�b�_�ɂ���w�b�_�Ǝ��ۂ̃f�[�^�̃w�b�_��
        ��v���Ă��Ȃ���� �G���[��Ԃ� */

    if(addr[0] != sector->headm || addr[1] != sector->headv) {
        printf("header is wrong!\n");
        StFreeRing(addr);
        return(0);
    }

    if(sector->frameCount == END_FRAME)
      {
        Rewind_Switch = 1;
      }

    /* ��ʂ̉𑜓x�� �O�̃t���[���ƈႤ�Ȃ�� ClearImage �����s */
    if(width != sector->width || height != sector->height) {

        RECT    rect;
        setRECT(&rect, 0, 0, 640 * PPW, 480);
        ClearImage(&rect, 0, 0, 0);

        width  = sector->width;
        height = sector->height;
    }

    /* �~�j�w�b�_�ɍ��킹�ăf�R�[�h����ύX���� */
    dec->rect[0].w = dec->rect[1].w = width*PPW;
    dec->rect[0].h = dec->rect[1].h = height;
    dec->slice.h   = height;

    return(addr);
}

static strSync(DECENV *dec, int mode)
{
    volatile u_long cnt = WAIT_TIME;

    while (dec->isdone == 0) {
        if (--cnt == 0) {
            /* timeout: �����I�ɐؑւ��� */
            printf("time out in decoding !\n");
            dec->isdone = 1;
            dec->rectid = dec->rectid? 0: 1;
            dec->slice.x = dec->rect[dec->rectid].x;
            dec->slice.y = dec->rect[dec->rectid].y;
        }
    }
    dec->isdone = 0;
}


/*
 * CDROM��loc����READ����B
 */
static strKickCD(CdlLOC *loc)
{
    /* �ړI�n�܂� Seek ���� */
    while (CdControl(CdlSeekL, (u_char *)loc, 0) == 0);

    /* �X�g���[�~���O���[�h��ǉ����ăR�}���h���s */
    while(CdRead2(0x100|CdlModeSpeed|CdlModeRT) == 0);
}
