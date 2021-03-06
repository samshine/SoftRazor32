#include "mspcode.h"

/*
Reference From Semi VB Decompiler
Thank: vbgamer45
http://www.vbforums.com/member.php?51546-vbgamer45
*/

#define MSPC_MAX_BUFFER     512

#ifndef __cplusplus
#define inline __inline
#endif

extern const VB_PCODE_INSTRUCTIONSET PCodeStdInst[251];
extern const VB_PCODE_INSTRUCTIONSET * PCodeLeadInst[5];
extern BOOL GetPropertyText(GUID guid, WORD VTOffset, PWCHAR szNote, DWORD slNote);

static inline BYTE GetByteFormPoint(void * pany)
{
  return *(BYTE *)pany;
}

static inline int16_t GetSI16FormP16(void * pint16)
{
  return *(int16_t *)pint16;
}

static inline uint16_t GetUI16FormP16(void * pint16)
{
  return *(uint16_t *)pint16;
}

static inline uint32_t GetUI32FormPSI16(void * pint16)
{
  return (uint32_t)*(int16_t *)pint16;
}

static inline uint32_t GetUI32FormPUI16(void * pint16)
{
  return (uint32_t)*(uint16_t *)pint16;
}

static inline int32_t GetSI32FormPSI16(void * pint16)
{
  return (int32_t)*(int16_t *)pint16;
}

static inline int32_t GetSI32FormPUI16(void * pint16)
{
  return (int32_t)*(uint16_t *)pint16;
}

static inline int32_t GetSI32FormP32(void * pint32)
{
  return *(int32_t *)pint32;
}

static inline uint32_t GetUI32FormP32(void * pint32)
{
  return *(uint32_t *)pint32;
}

BOOL NTAPI GetProcName(UINT hMod, UINT pAddr, PCHAR * oppChr, PCHAR * pmName)
{
  if (!hMod || !pAddr || !oppChr) return FALSE;

  PIMAGE_NT_HEADERS32 nthdr = (PIMAGE_NT_HEADERS32)(hMod + ((PIMAGE_DOS_HEADER)hMod)->e_lfanew);
  if (nthdr->FileHeader.SizeOfOptionalHeader < 0x70) return FALSE;
  if (nthdr->OptionalHeader.NumberOfRvaAndSizes < 2) return FALSE;
  PIMAGE_IMPORT_DESCRIPTOR pImpDir = (PIMAGE_IMPORT_DESCRIPTOR)(hMod + nthdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
  PIMAGE_THUNK_DATA32 pOrgThunk;
  PIMAGE_THUNK_DATA32 pThunk;
  uint32_t i = 0, j;

  while (pImpDir[i].OriginalFirstThunk)
  {
    j = 0;
    pOrgThunk = (PIMAGE_THUNK_DATA32)(hMod + pImpDir[i].OriginalFirstThunk);
    pThunk = (PIMAGE_THUNK_DATA32)(hMod + pImpDir[i].FirstThunk);

    while (pOrgThunk[j].u1.AddressOfData)
    {
      if (pThunk[j].u1.Function == pAddr)
      {
        if (IMAGE_SNAP_BY_ORDINAL32(pOrgThunk[j].u1.Ordinal))
          *oppChr = (PCHAR)(IMAGE_ORDINAL32(pOrgThunk[j].u1.Ordinal));
        else
          *oppChr = (PCHAR)((PIMAGE_IMPORT_BY_NAME)(hMod + pOrgThunk[j].u1.AddressOfData))->Name;

        *pmName = (PCHAR)(hMod + pImpDir[i].Name);
        return TRUE;
      }
      j++;
    }
    i++;
  }
  return FALSE;
}

static void NTAPI MakeArg(int32_t ival, wchar_t * iText, size_t maxc)
{
  WCHAR stmp[16];

  if (ival < 0)
    swprintf_s(stmp, 16, L"var_%X", -ival);
  else
    swprintf_s(stmp, 16, L"arg_%X", ival);

  wcscat_s(iText, maxc, stmp);
}

/* Not used */
static void NTAPI MakeAddr(PPDO pcdo, uint32_t uval)
{
#define PRIV_BUFLEN       32
  int32_t si32;
  WCHAR stmp[PRIV_BUFLEN];

  stmp[0] = 0;

  if (uval >= pcdo->mod_base && (uval < pcdo->mod_base + pcdo->mod_size))
  {
    if (*(uint8_t *)uval == 0xBA && *(uint8_t *)(uval + 5) == 0xB9)
    {
      si32 = GetSI32FormP32((void *)(uval + 1));
      if (!pcdo->func_qs || !pcdo->func_qs(si32, stmp, MSPC_MAX_BUFFER))
        swprintf_s(stmp, PRIV_BUFLEN, L"proc_%08X", si32);
    }
    else if (*(uint16_t *)uval == 0x25FF)    //jmp dword ptr
    {
      si32 = GetSI32FormP32((void *)(uval + 2));
      swprintf_s(stmp, PRIV_BUFLEN, L"eimp_%08X", si32);
    }
    else
      swprintf_s(stmp, PRIV_BUFLEN, L"unkn_%08X", uval);
  }
  else
    swprintf_s(stmp, PRIV_BUFLEN, L"????_%08X", uval);

  wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
#undef PRIV_BUFLEN
}

static void NTAPI MakeAddrToVB(PPDO pcdo, uint32_t ival)
{
  int32_t si32;
  WCHAR stmp[MSPC_MAX_BUFFER];

  if (ival >= pcdo->mod_base && (ival < pcdo->mod_base + pcdo->mod_size))
  {
    stmp[0] = 0;

    if (PCDM_TESTBYTE(ival, 0xBA) && PCDM_TESTBYTE(ival + 5, 0xB9))
    {
      si32 = GetSI32FormP32((void *)(ival + 1));
      if (!pcdo->func_qs || !pcdo->func_qs(si32, stmp, MSPC_MAX_BUFFER))
        swprintf_s(stmp, MSPC_MAX_BUFFER, L"proc_%08X", si32);
    }
    else if (PCDM_TESTWORD(ival, 0x25FF))  //jmp dword ptr
    {
      int ImpAddr;
      PCHAR pImp = 0;
      PCHAR pmName = 0;
      size_t bval;

      si32 = GetSI32FormP32((void *)(ival + 2));

      ImpAddr = *(int *)si32;

      if (GetProcName(pcdo->mod_base, ImpAddr, &pImp, &pmName))
      {
        mbstowcs_s(&bval, stmp, MSPC_MAX_BUFFER, pmName, _TRUNCATE);

        if ((uint32_t)pImp > 0xFFFF)
        {
          wcscat_s(stmp, MSPC_MAX_BUFFER, L"->");
          wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
          mbstowcs_s(&bval, stmp, MSPC_MAX_BUFFER, pImp, _TRUNCATE);
        }
        else
        {
          wcscat_s(stmp, MSPC_MAX_BUFFER, L"#>");
          wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
          swprintf_s(stmp, MSPC_MAX_BUFFER, L"%X", pImp);
        }
      }
      else if (!pcdo->func_qs || !pcdo->func_qs(si32, stmp, MSPC_MAX_BUFFER))
        swprintf_s(stmp, MSPC_MAX_BUFFER, L"eimp_%08X", si32);
    }
    else
      swprintf_s(stmp, MSPC_MAX_BUFFER, L"unkn_%08X", ival);
  }
  else
  {
    swprintf_s(stmp, MSPC_MAX_BUFFER, L"????_%08X", ival);
  }
  wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
}

static void NTAPI ReturnApiCall(PPDO pcdo, uint32_t iAddr)
{
  uint32_t ui32, ui32tmp;
  size_t bval;
  WCHAR stmp[MSPC_MAX_BUFFER];

  /* mov eax, dword ptr [xxxxxxxx] ; push xxxxxxxx */
  if (PCDM_TESTBYTE(iAddr, 0xA1) && PCDM_TESTBYTE(iAddr + 11, 0x68))
  {
    ui32 = GetUI32FormP32((void *)(iAddr + 12));
    if ((ui32 >= pcdo->mod_base) && (ui32 < pcdo->mod_base + pcdo->mod_size))
    {
      ui32tmp = GetUI32FormP32((void *)ui32);
      mbstowcs_s(&bval, stmp, MSPC_MAX_BUFFER, (char *)ui32tmp, _TRUNCATE);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);

      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, L"->");

      ui32tmp = GetUI32FormP32((void *)(ui32 + 4));
      mbstowcs_s(&bval, stmp, MSPC_MAX_BUFFER, (char *)ui32tmp, _TRUNCATE);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
      return;
    }
  }
  MakeAddrToVB(pcdo, iAddr);
}

/*
pcf_init_decode_object_basic
初始化解码对象
返回值:
0为成功,非0失败
*/
int pcf_init_decode_object_basic(PPDO pcdo, PVBPDI pdi, PWCHAR pstr, uint32_t slen, uint32_t mb, uint32_t ms)
{
  if (!pcdo) return -1;
  if (!pdi || !pdi->ProcSize) return -2;
  if (!pstr || !slen) return -3;
  if (!mb || !ms) return -4;

  memset(pcdo, 0, sizeof(PDO));

  pcdo->inp_opbuf = (PBYTE)((uint32_t)pdi - pdi->ProcSize);
  pcdo->inp_count = pdi->ProcSize;
  pcdo->flag_user = PCDF_INCREMENTCOUNT;
  pcdo->sz_mnem = pstr;
  pcdo->sl_mnem = slen;
  pcdo->mod_base = mb;
  pcdo->mod_size = ms;
  pcdo->vb_pdi = pdi;
  pcdo->bl_init = TRUE;
  return 0;
}

BOOL pcf_alloc_mapping(PPDO pcdo)
{
  if (!pcdo) return FALSE;
  if (!pcdo->vb_pdi) return FALSE;
  if (!pcdo->vb_pdi->ProcSize) return FALSE;

  if (pcdo->pri_opmap)
    free(pcdo->pri_opmap);

  pcdo->pri_opmap = (PBYTE)calloc(pcdo->vb_pdi->ProcSize, 1);
  pcdo->pri_jmpad = (PDWORD)calloc(10, sizeof(DWORD));
  pcdo->cnt_jmpad = 10;

  if (pcdo->pri_opmap && pcdo->pri_jmpad)
    return TRUE;

  if (pcdo->pri_opmap)
    free(pcdo->pri_opmap);

  if (pcdo->pri_jmpad)
    free(pcdo->pri_jmpad);

  pcdo->pri_opmap = NULL;
  pcdo->pri_jmpad = NULL;
  pcdo->cnt_jmpad = 0;
  return FALSE;
}

BOOL pcf_free_mapping(PPDO pcdo)
{
  if (!pcdo) return FALSE;
  if (!pcdo->pri_opmap && !pcdo->pri_jmpad) return FALSE;

  if (pcdo->pri_opmap)
    free(pcdo->pri_opmap);

  if (pcdo->pri_jmpad)
    free(pcdo->pri_jmpad);

  pcdo->pri_opmap = NULL;
  pcdo->pri_jmpad = NULL;
  pcdo->cnt_jmpad = 0;
  return TRUE;
}

/*
pcfp_chk_vp_signed
检查变参长度的符号
返回值:
有符号为TRUE,其它情况为FALSE
*/
static BOOL pcfp_chk_vp_signed(LPCWCH szParam)
{
  WCHAR wtmp;

  if (!szParam) return FALSE;

  while (wtmp = *szParam)
  {
    if (wtmp == L'%')
    {
      if (szParam[1] == L'j')
        return TRUE;
      else
        return FALSE;
    }
    szParam++;
  }
  return FALSE;
}

/*
pcfp_get_fixed_length
获取变长指令中,变参长度描述WORD后的固定参数的长度 [私有]
返回值:
固定参数的长度
*/
static uint32_t pcfp_get_fixed_length(LPCWCH szParam)
{
  uint32_t flen = 0;
  BOOL vpbl = FALSE;

  if (!szParam) return 0;

  while (*szParam)
  {
    if (*szParam == L'%')
    {
      szParam++;

      if (*szParam == 0)
        return flen;

      if (vpbl)     //开始统计的布尔值
      {
        //变参数组开始处截断统计
        if (*szParam == L'p')
          return flen;

        switch (*szParam)
        {
        case L'1':
        case L'b':
          flen++;
          break;
        case L'2':
        case L'w':
        case L'a':
        case L'c':
        case L'l':
        case L's':
        case L't':
        case L'x':
          flen += 2;
          break;
        case L'4':
        case L'e':
        case L'v':
          flen += 4;
          break;
        case L'8':
          flen += 8;
          break;
        }
      }
      else
      {
        //从变参长度描述WORD开始统计
        if (*szParam == L'j' || *szParam == L'k')
          vpbl = TRUE;
      }
    }
    szParam++;
  }
  return flen;
}


/*
pcf_decode
解码pcode
返回值:
0为成功,非0失败
返回值区间:
-32 ~ 32
*/
int pcf_decode(PPDO pcdo)
{
  uint32_t idx, max;
  PBYTE pbuf;
  BYTE curop, leidx;

  if (!pcdo) return -1;
  if (!pcdo->inp_opbuf || !pcdo->inp_count) return -2;
  if (pcdo->inp_idx > pcdo->inp_count) return -3;
  if (pcdo->inp_idx == pcdo->inp_count) return 1;   //End Of Buffer

  idx = pcdo->inp_idx;
  max = pcdo->inp_count;
  pbuf = pcdo->inp_opbuf;
  pcdo->bk_idx = idx;
  pcdo->ib_std = curop = pbuf[idx];     //[o][?][?]

  if (curop >= 0xFB)   //存在引导指令前缀
  {
    if (++idx >= max) return -4;    //[*][o][?]
    if (curop == 0xFF && pbuf[idx] >= 0x47) return -5;

    leidx = curop - 0xFB;
    curop = pbuf[idx];
    pcdo->ib_lead = curop;

    if (PCDM_CHKVPARAM(PCodeLeadInst[leidx][curop].inst_len))  //变长指令
    {
      uint32_t vlen;      //后缀长度
      uint32_t flen;      //定参长度

      if (idx + 3 > max) return -6;

      if (pcfp_chk_vp_signed(PCodeLeadInst[leidx][curop].text_param))  //有符号
        vlen = (uint32_t)GetSI32FormPSI16(&pbuf[++idx]);
      else    //无符号或其它情况
        vlen = (uint32_t)*(uint16_t *)(&pbuf[++idx]);

      flen = pcfp_get_fixed_length(PCodeLeadInst[leidx][curop].text_param);

      if ((vlen > 0xFFFF) && pcdo->func_eh)
      {
        PDO pdo = *pcdo;

        if (pcdo->func_eh(PCDEC_VPTOOLONG, PCDEF_VALID_CONT_FAIL, &pdo, &vlen) == PCDER_FAILED)
          return -7;
      }

      if (vlen & 1)
      {
        pcdo->lastval |= PCEC_NOTALIGNED;
        vlen--;
      }

      if (flen && (flen < vlen))     //存在固定长度,并小于变参长度
      {
        pcdo->lastval |= PCEC_FIXEDERR;
        return -8;
      }

      idx += 2;
      pcdo->len_fixed = 1 + (uint32_t)-(PCodeLeadInst[leidx][curop].inst_len);
      pcdo->len_total = pcdo->len_fixed + vlen;
      pcdo->inst_type = PCodeLeadInst[leidx][curop].inst_type;
      idx += vlen;
    }
    else        //定长指令
    {
      idx += PCodeLeadInst[leidx][curop].inst_len;
      pcdo->len_fixed = 1 + PCodeLeadInst[leidx][curop].inst_len;
      pcdo->len_total = pcdo->len_fixed;
      pcdo->inst_type = PCodeLeadInst[leidx][curop].inst_type;
    }
  }
  else      //标准指令前缀
  {
    if (PCDM_CHKVPARAM(PCodeStdInst[curop].inst_len))     //变长指令
    {
      uint32_t vlen;      //后缀长度
      uint32_t flen;      //定参长度

      if (idx + 3 > max) return -9;

      if (pcfp_chk_vp_signed(PCodeStdInst[curop].text_param))  //有符号
        vlen = (uint32_t)GetSI32FormPSI16(&pbuf[++idx]);
      else    //无符号或其它情况
        vlen = (uint32_t)*(uint16_t *)(&pbuf[++idx]);

      flen = pcfp_get_fixed_length(PCodeStdInst[curop].text_param);

      if ((vlen > 0xFFFF) && pcdo->func_eh)
      {
        PDO pdo = *pcdo;

        if (pcdo->func_eh(PCDEC_VPTOOLONG, PCDEF_VALID_CONT_FAIL, &pdo, NULL) == PCDER_FAILED)
          return -10;
      }

      if (vlen & 1)
      {
        pcdo->lastval |= PCEC_NOTALIGNED;
        vlen--;
      }

      if (flen && (flen < vlen))     //存在固定长度,并小于变参长度
      {
        pcdo->lastval |= PCEC_FIXEDERR;
        return -11;
      }

      idx += 2;
      pcdo->len_fixed = (uint32_t)-(PCodeStdInst[curop].inst_len);
      pcdo->len_total = pcdo->len_fixed + vlen;
      pcdo->inst_type = PCodeStdInst[curop].inst_type;
      idx += vlen;
    }
    else      //定长指令
    {
      idx += PCodeStdInst[curop].inst_len;
      pcdo->len_fixed = PCodeStdInst[curop].inst_len;
      pcdo->len_total = pcdo->len_fixed;
      pcdo->inst_type = PCodeStdInst[curop].inst_type;
    }
  }

  if (idx > max)
  {
    pcdo->lastval |= PCEC_OVERFLOW;
    return -12;
  }
  else if (idx == max)
    pcdo->lastval |= PCEC_DECODEOVER;

  if (PCDM_CHKFLAG(pcdo->flag_user, PCDF_INCREMENTCOUNT))
    pcdo->inp_idx = idx;

  return 0;
}

/*
pcfp_printf_arg
打印参数 [私有]
返回值区间:
[-33 ~ -64] ~ [33 ~ 64]
*/
static int pcfp_printf_arg(PPDO pcdo, PLONG cpool, LONG ocpool)
{
  if (pcdo->bk_idx + pcdo->len_total > pcdo->inp_count)
    return -33;
  if (!pcdo->vb_pdi || !pcdo->vb_pdi->lpTableInfo) return -34;
  if (!cpool) return -35;

  PBYTE vbuf;
  LPCWCH tprm;
  uint64_t ui64;
  uint32_t vidx = 0, vplen = 0, vmax, ui32;
  int32_t si32, si32tmp;
  uint16_t ui16, ui16_1;
  BOOL vpbl = FALSE;
  WCHAR stmp[MSPC_MAX_BUFFER];

  if (pcdo->ib_std >= 0xFB)    //存在引导指令
  {
    vbuf = &pcdo->inp_opbuf[pcdo->bk_idx + 2];
    tprm = PCodeLeadInst[pcdo->ib_std - 0xFB][pcdo->ib_lead].text_param;
    vmax = pcdo->len_total - 2;
  }
  else    //标准指令
  {
    vbuf = &pcdo->inp_opbuf[pcdo->bk_idx + 1];
    tprm = PCodeStdInst[pcdo->ib_std].text_param;
    vmax = pcdo->len_total - 1;
  }

  while (*tprm)
  {
    if (*tprm != L'%')
    {
      wcsncat_s(pcdo->sz_mnem, pcdo->sl_mnem, tprm, 1);
      tprm++;
      continue;
    }

    if (*(++tprm) == 0) break;

    switch (*tprm)
    {
    case L'1':
    case L'b':
      if (vidx + 1 > vmax) return -40;
      ui32 = (uint32_t)*(uint8_t *)&vbuf[vidx];
      swprintf_s(stmp, MSPC_MAX_BUFFER, L"db_%02X", ui32);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
      vidx++;
      break;
    case L'2':
    case L'w':
      if (vidx + 2 > vmax) return -41;
      ui32 = (uint32_t)*(uint16_t *)&vbuf[vidx];
      swprintf_s(stmp, MSPC_MAX_BUFFER, L"dw_%04X", ui32);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
      vidx += 2;
      break;
    case L'4':
      if (vidx + 4 > vmax) return -42;
      ui32 = *(uint32_t *)&vbuf[vidx];
      swprintf_s(stmp, MSPC_MAX_BUFFER, L"dd_%08X", ui32);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
      vidx += 4;
      break;
    case L'8':
      if (vidx + 8 > vmax) return -43;
      ui64 = *(uint64_t *)&vbuf[vidx];
      swprintf_s(stmp, MSPC_MAX_BUFFER, L"dq_%016I64X", ui64);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
      vidx += 8;
      break;
    case L'a':
      if (vidx + 2 > vmax) return -44;
      si32 = GetSI32FormPUI16(&vbuf[vidx]);
      MakeArg(si32, pcdo->sz_mnem, pcdo->sl_mnem);
      vidx += 2;
      break;
    case L'c':
      if (vidx + 2 > vmax) return -45;
      ui16 = GetUI16FormP16(&vbuf[vidx]);
      si32 = ui16 * 4 + *cpool;
      ui32 = GetUI32FormP32((void *)si32);
      MakeAddrToVB(pcdo, ui32);
      vidx += 2;
      break;
    case L'e':    //**Not used**
      if (vidx + 4 > vmax) return -46;
      ui16 = GetUI16FormP16(&vbuf[vidx]);
      si32 = ui16 * 4 + *cpool;
      si32 += GetSI32FormP32(&vbuf[vidx + 2]);
      ui32 = GetUI32FormPSI16((void *)si32);
      MakeAddr(pcdo, ui32);
      vidx += 4;
      break;
    case L'j':
      if (vidx + 2 > vmax) return -47;

      if (vpbl && pcdo->func_eh)
      {
        PDO pdo = *pcdo;
        int erval = pcdo->func_eh(PCDEC_REPEATVPLEN, PCDEF_VALID_ALL, &pdo, NULL);

        if (erval == PCDER_CONTINUE)
          break;
        else if (erval == PCDER_FAILED)
          return -60;
      }
      vplen = (uint32_t)GetSI32FormPSI16(&vbuf[vidx]);
      vpbl = TRUE;
      vidx += 2;
      break;
    case L'k':
      if (vidx + 2 > vmax) return -48;

      if (vpbl && pcdo->func_eh)
      {
        PDO pdo = *pcdo;
        int erval = pcdo->func_eh(PCDEC_REPEATVPLEN, PCDEF_VALID_ALL, &pdo, NULL);

        if (erval == PCDER_CONTINUE)
          break;
        else if (erval == PCDER_FAILED)
          return -61;
      }
      vplen = GetUI32FormPUI16(&vbuf[vidx]);
      vpbl = TRUE;
      vidx += 2;
      break;
    case L'l':
      if (vidx + 2 > vmax) return -49;
      pcdo->pri_offset = *(uint16_t *)&vbuf[vidx];
      ui32 = (uint32_t)pcdo->inp_opbuf + pcdo->pri_offset;
      swprintf_s(stmp, MSPC_MAX_BUFFER, L"loc_%08X", ui32);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
      vidx += 2;
      break;
    case L'p':
      vplen >>= 1;
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, L" { ");
      while (vplen)
      {
        if (vidx + 2 > vmax)
        {
          wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, L" ..!.. }");
          return -50;
        }
        swprintf_s(stmp, MSPC_MAX_BUFFER, L"%02X", *(uint16_t *)&vbuf[vidx]);
        wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
        vidx += 2;
        vplen--;

        if (vplen)
          wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, L", ");
      }
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, L" }");
      break;
    case L's':
      if (vidx + 2 > vmax) return -51;
      ui16 = GetUI16FormP16(&vbuf[vidx]);
      si32 = ui16 * 4 + *cpool;
      ui32 = GetUI32FormP32((void *)si32);

      swprintf_s(stmp, MSPC_MAX_BUFFER, L"vstr_%08X", ui32);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);

      if (pcdo->sz_note && pcdo->sl_note)
      {
        wcscpy_s(pcdo->sz_note, pcdo->sl_note, L" \"");
        swprintf_s(stmp, MSPC_MAX_BUFFER, L"%s", ui32);
        wcscat_s(stmp, MSPC_MAX_BUFFER, L"\" ");
        wcscat_s(pcdo->sz_note, pcdo->sl_note, stmp);
      }

      vidx += 2;
      break;
    case L't':
      if (vidx + 2 > vmax) return -52;
      ui16 = GetUI16FormP16(&vbuf[vidx]);    //FIdx  ?
      ui32 = ui16 * 4 + ocpool;
      ui32 = *(uint32_t *)ui32;
      swprintf_s(stmp, MSPC_MAX_BUFFER, L"constpool_%08X", ui32);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
      *cpool = ui32;
      vidx += 2;
      break;
    case L'x':   //External Api
      if (vidx + 2 > vmax) return -53;
      ui16 = GetSI16FormP16(&vbuf[vidx]);
      si32 = ui16 * 4 + *cpool;
      ui32 = GetUI32FormP32((void *)si32);
      ReturnApiCall(pcdo, ui32);
      vidx += 2;
      break;
    case L'v':
      if (vidx + 4 > vmax) return -54;
      ui16_1 = GetUI16FormP16(&vbuf[vidx]);   //VTable Offset
      vidx += 2;
      ui16 = GetUI16FormP16(&vbuf[vidx]);
      si32tmp = GetSI32FormP32((void *)(ui16 * 4 + *cpool));    //Pointer GUID

      swprintf_s(stmp, MSPC_MAX_BUFFER, L"<VTOS=%04X,GUID=%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X>", 
        (uint32_t)ui16_1, (uint32_t)((GUID *)si32tmp)->Data1, (uint32_t)((GUID *)si32tmp)->Data2,
        (uint32_t)((GUID *)si32tmp)->Data3, (uint32_t)((GUID *)si32tmp)->Data4[0], (uint32_t)((GUID *)si32tmp)->Data4[1],
        (uint32_t)((GUID *)si32tmp)->Data4[2], (uint32_t)((GUID *)si32tmp)->Data4[3], (uint32_t)((GUID *)si32tmp)->Data4[4],
        (uint32_t)((GUID *)si32tmp)->Data4[5], (uint32_t)((GUID *)si32tmp)->Data4[6], (uint32_t)((GUID *)si32tmp)->Data4[7]);
      wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);

      if (pcdo->sz_note && pcdo->sl_note)
        GetPropertyText(*(GUID *)si32tmp, ui16_1, pcdo->sz_note, pcdo->sl_note);

      vidx += 2;
      break;
    default:
      if (pcdo->func_eh)
      {
        PDO pdo = *pcdo;

        if (pcdo->func_eh(PCDEC_INVALIDFORMATCHAR, PCDEF_VALID_RESULTS, &pdo, NULL) == PCDER_RESULT1)
          wcsncat_s(pcdo->sz_mnem, pcdo->sl_mnem, tprm, 1);
      }
      break;
    }
    tprm++;
  }

  return 0;
}

/*
P-Code 反汇编
*/
int pcf_disassemble(PPDO pcdo)
{
#define PRIV_BUFLEN       128

  int iret;
  LPCCH t_inst;
  LPCWCH t_para;
  size_t bval;
  BYTE curop;       //current op
  BYTE leidx;
  WCHAR stmp[PRIV_BUFLEN];
 
  if (!pcdo) return -65;
  if (!pcdo->inp_opbuf || !pcdo->inp_count) return -66;
  if (!pcdo->sz_mnem || pcdo->sl_mnem == 0) return -67;
  if (pcdo->inp_idx > pcdo->inp_count) return -68;      //End Of Buffer
  if (pcdo->inp_idx == pcdo->inp_count) return 1;

  /* 解码指令 */
  iret = pcf_decode(pcdo);
  if (iret != 0) return iret;

  /* 助记符文本重置 */
  pcdo->sz_mnem[0] = 0;
  /* 注释文本重置 */
  if (pcdo->sl_note && pcdo->sz_note) pcdo->sz_note[0] = 0;

  /* 初始化操作 */
  if (pcdo->bl_init)
  {
    pcdo->stk_cpol = pcdo->org_cpol = pcdo->vb_pdi->lpTableInfo->ConstPool;
    pcdo->bl_init = FALSE;
  }

  /* 取当前指令首字节 */
  curop = pcdo->ib_std;
  if (curop >= 0xFB)   //Lead* 引导指令
  {
    leidx = curop - 0xFB;
    curop = pcdo->ib_lead;

    t_inst = PCodeLeadInst[leidx][curop].text_inst;
    t_para = PCodeLeadInst[leidx][curop].text_param;
    swprintf_s(pcdo->sz_mnem, pcdo->sl_mnem, L"#%1u:", (uint32_t)leidx);
  }
  else
  {
    t_inst = PCodeStdInst[curop].text_inst;
    t_para = PCodeStdInst[curop].text_param;
  }

  /* 指令文本 */
  if (mbstowcs_s(&bval, stmp, PRIV_BUFLEN, t_inst, _TRUNCATE))
    return -70;
  wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, stmp);
  wcscat_s(pcdo->sz_mnem, pcdo->sl_mnem, L" ");

  /* 打印参数文本 */
  if (t_para)
  {
    if ((pcdo->inst_type & 0xFF) == PCT0_IDX)
      iret = pcfp_printf_arg(pcdo, &(pcdo->stk_cpol), pcdo->vb_pdi->lpTableInfo->ConstPool);
    else
      iret = pcfp_printf_arg(pcdo, &(pcdo->org_cpol), pcdo->vb_pdi->lpTableInfo->ConstPool);

    if (iret != 0) return iret;
  }

  if (PCDM_CHKFLAG(pcdo->lastval, PCEC_DECODEOVER)) return 2;
  if (PCDM_CHKFLAG(pcdo->inst_type, PCFH_ENDPROC)) return 3;

  return 0;
#undef PRIV_BUFLEN
}

/*
pcfp_adsort
DWORD 冒泡排序
*/
static void pcfp_adsort(PDWORD pdwad, uint32_t dwcnt)
{
  uint32_t i, j;
  DWORD dwtmp;

  for (i = 0; i < dwcnt; i++)
  {
    for (j = 1; j < dwcnt - i; j++)
    {
      if (pdwad[j])
      {
        if ((pdwad[j - 1] == 0) || (pdwad[j - 1] > pdwad[j]))
        {
          dwtmp = pdwad[j - 1];
          pdwad[j - 1] = pdwad[j];
          pdwad[j] = dwtmp;
        }
      }
    }
  }
}

/* 
pcfp_set_address
设置地址索引
*/
static BOOL pcfp_set_address(PPDO pcdo, DWORD indw)
{
  uint32_t i;

  if (!pcdo || !pcdo->pri_jmpad || !pcdo->cnt_jmpad)
    return FALSE;

  for (i = 0; i < pcdo->cnt_jmpad; i++)
  {
    if (pcdo->pri_jmpad[i] == 0)
    {
      pcdo->pri_jmpad[i] = indw;
      pcfp_adsort(pcdo->pri_jmpad, pcdo->cnt_jmpad);
      return TRUE;
    }
    else if (pcdo->pri_jmpad[i] == indw)
      return TRUE;
  }

  pcdo->pri_jmpad = (PDWORD)_recalloc(pcdo->pri_jmpad, pcdo->cnt_jmpad + 4, sizeof(DWORD));
  if (!pcdo->pri_jmpad)
  {
    pcdo->cnt_jmpad = 0;
    return FALSE;
  }

  pcdo->pri_jmpad[pcdo->cnt_jmpad] = indw;
  pcdo->cnt_jmpad += 4;
  pcfp_adsort(pcdo->pri_jmpad, pcdo->cnt_jmpad);
  return TRUE;
}

/*
pcfp_get_address
获取未处理的地址索引
*/
static BOOL pcfp_get_address(PPDO pcdo, PDWORD pout)
{
  uint32_t i;

  if (!pcdo || !pcdo->pri_jmpad || !pcdo->cnt_jmpad || !pout)
    return FALSE;

  for (i = 0; i < pcdo->cnt_jmpad; i++)
  {
    if (pcdo->pri_jmpad[i] != 0)
    {
      *pout = pcdo->pri_jmpad[i];
      pcdo->pri_jmpad[i] = 0;
      pcfp_adsort(pcdo->pri_jmpad, pcdo->cnt_jmpad);
      return TRUE;
    }
  }

  return FALSE;
}

/*
pcf_disasm_proc
根据指令流程反汇编过程
*/
int pcf_disasm_proc(PPDO pcdo)
{
  int iret;
  un_dword dwtmp;

  if (!pcdo) return -97;
  if (!pcdo->pri_opmap) return -98;
  if (!pcdo->pri_jmpad || !pcdo->cnt_jmpad) return -99;

  pcdo->flag_user |= PCDF_INCREMENTCOUNT;
  iret = pcf_disassemble(pcdo);

  if (iret < 0 || iret == 1)
    return iret;

  /* 标记已解码的字节 */
  memset(&pcdo->pri_opmap[pcdo->bk_idx], 1, pcdo->len_total);

  if (PCDM_CHKFLAG(pcdo->inst_type, PCFH_BRANCH))   //无条件分支
  {
    if (pcdo->pri_offset >= pcdo->inp_count)
      return -100;

    if (pcdo->pri_opmap[pcdo->pri_offset] == 0)   //分支地址未解码
      pcdo->inp_idx = pcdo->pri_offset;
    else
    {
      while (pcfp_get_address(pcdo, &dwtmp.dw))
      {
        if (pcdo->pri_opmap[dwtmp.wd[0]] == 0)
        {
          pcdo->inp_idx = dwtmp.wd[0];
          return 0;
        }
      }

      return 2;
    }
  }
  else if (PCDM_CHKFLAG(pcdo->inst_type, PCFH_CONDBRAN))  //有条件分支
  {
    if (pcdo->pri_offset >= pcdo->inp_count)
      return -101;

    if (pcdo->pri_opmap[pcdo->pri_offset] == 0)   //分支地址未解码
    {
      dwtmp.wd[0] = pcdo->pri_offset;
      dwtmp.wd[1] = 1;

      if (!pcfp_set_address(pcdo, dwtmp.dw))
        return -102;
    }
  }
  else if (iret == 2 || iret == 3)  //end of buffer || end proc
  {
    while (pcfp_get_address(pcdo, &dwtmp.dw))
    {
      if (pcdo->pri_opmap[dwtmp.wd[0]] == 0)
      {
        pcdo->inp_idx = dwtmp.wd[0];
        return 0;
      }
    }

    return 2;
  }

  return iret;
}

BOOL pcf_decodedcount(PPDO pcdo, uint32_t * dednum, uint32_t * mapmax)
{
  uint32_t dedw = 0, i;

  if (!pcdo || !dednum || !mapmax)
    return FALSE;

  if (!pcdo->pri_opmap || !pcdo->inp_count)
    return FALSE;

  *mapmax = pcdo->inp_count;
  for (i = 0; i < pcdo->inp_count;i++)
  {
    if (pcdo->pri_opmap[i])
      dedw++;
  }
  *dednum = dedw;
  return TRUE;
}