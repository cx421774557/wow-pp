/*****************************************************************************/
/* SFileFindFile.cpp                      Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* A module for file searching within MPQs                                   */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 25.03.03  1.00  Lad  The first version of SFileFindFile.cpp               */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"

//-----------------------------------------------------------------------------
// Defines

#define LISTFILE_CACHE_SIZE 0x1000

//-----------------------------------------------------------------------------
// Private structure used for file search (search handle)

struct TMPQSearch;
typedef int (*MPQSEARCH)(TMPQSearch *, SFILE_FIND_DATA *);

// Used by searching in MPQ archives
struct TMPQSearch
{
    TMPQArchive * ha;                   // Handle to MPQ, where the search runs
    TFileEntry ** pSearchTable;         // Table for files that have been already found
    DWORD  dwSearchTableItems;          // Number of items in the search table
    DWORD  dwNextIndex;                 // Next file index to be checked
    DWORD  dwFlagMask;                  // For checking flag mask
    char   szSearchMask[1];             // Search mask (variable length)
};

//-----------------------------------------------------------------------------
// Local functions

static TMPQSearch * IsValidSearchHandle(HANDLE hFind)
{
    TMPQSearch * hs = (TMPQSearch *)hFind;

    if(hs != NULL && IsValidMpqHandle(hs->ha))
        return hs;

    return NULL;
}

bool CheckWildCard(const char * szString, const char * szWildCard)
{
    const char * szSubString;
    int nSubStringLength;
    int nMatchCount = 0;

    // When the mask is empty, it never matches
    if(szWildCard == NULL || *szWildCard == 0)
        return false;

    // If the wildcard contains just "*", then it always matches
    if(szWildCard[0] == '*' && szWildCard[1] == 0)
        return true;

    // Do normal test
    for(;;)
    {
        // If there is '?' in the wildcard, we skip one char
        while(*szWildCard == '?')
        {
            szWildCard++;
            szString++;
        }

        // If there is '*', means zero or more chars. We have to 
        // find the sequence after '*'
        if(*szWildCard == '*')
        {
            // More stars is equal to one star
            while(*szWildCard == '*' || *szWildCard == '?')
                szWildCard++;

            // If we found end of the wildcard, it's a match
            if(*szWildCard == 0)
                return true;

            // Determine the length of the substring in szWildCard
            szSubString = szWildCard;
            while(*szSubString != 0 && *szSubString != '?' && *szSubString != '*')
                szSubString++;
            nSubStringLength = (int)(szSubString - szWildCard);
            nMatchCount = 0;

            // Now we have to find a substring in szString,
            // that matches the substring in szWildCard
            while(*szString != 0)
            {
                // Calculate match count
                while(nMatchCount < nSubStringLength)
                {
                    if(AsciiToUpperTable[(BYTE)szString[nMatchCount]] != AsciiToUpperTable[(BYTE)szWildCard[nMatchCount]])
                        break;
                    if(szString[nMatchCount] == 0)
                        break;
                    nMatchCount++;
                }

                // If the match count has reached substring length, we found a match
                if(nMatchCount == nSubStringLength)
                {
                    szWildCard += nMatchCount;
                    szString += nMatchCount;
                    break;
                }

                // No match, move to the next char in szString
                nMatchCount = 0;
                szString++;
            }
        }
        else
        {
            // If we came to the end of the string, compare it to the wildcard
            if(AsciiToUpperTable[(BYTE)*szString] != AsciiToUpperTable[(BYTE)*szWildCard])
                return false;

            // If we arrived to the end of the string, it's a match
            if(*szString == 0)
                return true;

            // Otherwise, continue in comparing
            szWildCard++;
            szString++;
        }
    }
}

static DWORD GetSearchTableItems(TMPQArchive * ha)
{
    DWORD dwMergeItems = 0;

    // Loop over all patches
    while(ha != NULL)
    {
        // Append the number of files
        dwMergeItems += (ha->pHetTable != NULL) ? ha->pHetTable->dwEntryCount
                                                : ha->pHeader->dwBlockTableSize;
        // Move to the patched archive
        ha = ha->haPatch;
    }

    // Return the double size of number of items
    return (dwMergeItems | 1);
}

static bool FileWasFoundBefore(
    TMPQArchive * ha,
    TMPQSearch * hs,
    TFileEntry * pFileEntry)
{
    TFileEntry * pEntry;
    char * szRealFileName = pFileEntry->szFileName;
    DWORD dwStartIndex;
    DWORD dwNameHash;
    DWORD dwIndex;

    if(hs->pSearchTable != NULL && szRealFileName != NULL)
    {
        // If we are in patch MPQ, we check if patch prefix matches
        // and then trim the patch prefix
        if(ha->pPatchPrefix != NULL)
        {
            // If the patch prefix doesn't fit, we pretend that the file
            // was there before and it will be skipped
            if(_strnicmp(szRealFileName, ha->pPatchPrefix->szPatchPrefix, ha->pPatchPrefix->nLength))
                return true;

            szRealFileName += ha->pPatchPrefix->nLength;
        }

        // Calculate the hash to the table
        dwNameHash = HashString(szRealFileName, MPQ_HASH_NAME_A);
        dwStartIndex = dwIndex = (dwNameHash % hs->dwSearchTableItems);

        // The file might have been found before
        // only if this is not the first MPQ being searched
        if(ha->haBase != NULL)
        {
            // Enumerate all entries in the search table
            for(;;)
            {
                // Get the file entry at that position
                pEntry = hs->pSearchTable[dwIndex];
                if(pEntry == NULL)
                    break;

                if(pEntry->szFileName != NULL)
                {
                    // Does the name match?
                    if(!_stricmp(pEntry->szFileName, szRealFileName))
                        return true;
                }

                // Move to the next entry
                dwIndex = (dwIndex + 1) % hs->dwSearchTableItems;
                if(dwIndex == dwStartIndex)
                    break;
            }
        }

        // Put the entry to the table for later use
        hs->pSearchTable[dwIndex] = pFileEntry;
    }
    return false;
}

static inline bool FileEntryIsInvalid(
    TMPQArchive * ha,
    TFileEntry * pFileEntry)
{
    // Spazzler3 protector: Some files are clearly wrong
    return ((ha->dwFlags & MPQ_FLAG_MALFORMED) && (pFileEntry->dwCmpSize & 0xFFFF0000) >= 0x7FFF0000);
}

static TFileEntry * FindPatchEntry(TMPQArchive * ha, TFileEntry * pFileEntry)
{
    TFileEntry * pPatchEntry = NULL;
    TFileEntry * pTempEntry;
    char szFileName[MAX_PATH];
    LCID lcLocale = pFileEntry->lcLocale;

    // Go while there are patches
    while(ha->haPatch != NULL)
    {
        // Move to the patch archive
        ha = ha->haPatch;
        szFileName[0] = 0;

        // Prepare the prefix for the file name
        if(ha->pPatchPrefix != NULL)
            strcpy(szFileName, ha->pPatchPrefix->szPatchPrefix);
        strcat(szFileName, pFileEntry->szFileName);

        // Try to find the file there
        pTempEntry = GetFileEntryExact(ha, szFileName, lcLocale);
        if(pTempEntry != NULL)
            pPatchEntry = pTempEntry;
    }

    // Return the found patch entry
    return pPatchEntry;
}

// Performs one MPQ search
static int DoMPQSearch(TMPQSearch * hs, SFILE_FIND_DATA * lpFindFileData)
{
    TMPQArchive * ha = hs->ha;
    TFileEntry * pFileTableEnd;
    TFileEntry * pPatchEntry;
    TFileEntry * pFileEntry;
    const char * szFileName;
    HANDLE hFile;
    char szPseudoName[20];
    DWORD dwBlockIndex;
    size_t nPrefixLength;

    // Start searching with base MPQ
    while(ha != NULL)
    {
        // Now parse the file entry table in order to get all files.
        pFileTableEnd = ha->pFileTable + ha->dwFileTableSize;
        pFileEntry = ha->pFileTable + hs->dwNextIndex;

        // Get the length of the patch prefix (0 if none)
        nPrefixLength = (ha->pPatchPrefix != NULL) ? ha->pPatchPrefix->nLength : 0;

        // Parse the file table
        while(pFileEntry < pFileTableEnd)
        {
            // Increment the next index for subsequent search
            hs->dwNextIndex++;

            // Is it a file but not a patch file?
            if((pFileEntry->dwFlags & hs->dwFlagMask) == MPQ_FILE_EXISTS)
            {
                // Spazzler3 protector: Some files are clearly wrong
                if(!FileEntryIsInvalid(ha, pFileEntry))
                {                    
                    // Now we have to check if this file was not enumerated before
                    if(!FileWasFoundBefore(ha, hs, pFileEntry))
                    {
//                      if(pFileEntry != NULL && !_stricmp(pFileEntry->szFileName, "TriggerLibs\\NativeLib.galaxy"))
//                          DebugBreak();

                        // Find a patch to this file
                        pPatchEntry = FindPatchEntry(ha, pFileEntry);
                        if(pPatchEntry == NULL)
                            pPatchEntry = pFileEntry;

                        // Prepare the block index
                        dwBlockIndex = (DWORD)(pFileEntry - ha->pFileTable);

                        // Get the file name. If it's not known, we will create pseudo-name
                        szFileName = pFileEntry->szFileName;
                        if(szFileName == NULL)
                        {
                            // Open the file by its pseudo-name.
                            // This also generates the file name with a proper extension
                            sprintf(szPseudoName, "File%08u.xxx", (unsigned int)dwBlockIndex);
                            if(SFileOpenFileEx((HANDLE)hs->ha, szPseudoName, SFILE_OPEN_BASE_FILE, &hFile))
                            {
                                szFileName = (pFileEntry->szFileName != NULL) ? pFileEntry->szFileName : szPseudoName;
                                SFileCloseFile(hFile);
                            }
                        }

                        // If the file name is still NULL, we cannot include the file to search results
                        if(szFileName != NULL)
                        {
                            // Check the file name against the wildcard
                            if(CheckWildCard(szFileName + nPrefixLength, hs->szSearchMask))
                            {
                                // Fill the found entry. hash entry and block index are taken from the base MPQ
                                lpFindFileData->dwHashIndex  = pFileEntry->dwHashIndex;
                                lpFindFileData->dwBlockIndex = dwBlockIndex;
                                lpFindFileData->dwFileSize   = pPatchEntry->dwFileSize;
                                lpFindFileData->dwFileFlags  = pPatchEntry->dwFlags;
                                lpFindFileData->dwCompSize   = pPatchEntry->dwCmpSize;
                                lpFindFileData->lcLocale     = pPatchEntry->lcLocale;

                                // Fill the filetime
                                lpFindFileData->dwFileTimeHi = (DWORD)(pPatchEntry->FileTime >> 32);
                                lpFindFileData->dwFileTimeLo = (DWORD)(pPatchEntry->FileTime);

                                // Fill the file name and plain file name
                                strcpy(lpFindFileData->cFileName, szFileName + nPrefixLength);
                                lpFindFileData->szPlainName = (char *)GetPlainFileName(lpFindFileData->cFileName);
                                return ERROR_SUCCESS;
                            }
                        }
                    }
                }
            }

            pFileEntry++;
        }

        // If there is no more patches in the chain, stop it.
        // This also keeps hs->ha non-NULL, which is required
        // for freeing the handle later
        if(ha->haPatch == NULL)
            break;

        // Move to the next patch in the patch chain
        hs->ha = ha = ha->haPatch;
        hs->dwNextIndex = 0;
    }

    // No more files found, return error
    return ERROR_NO_MORE_FILES;
}

static void FreeMPQSearch(TMPQSearch *& hs)
{
    if(hs != NULL)
    {
        if(hs->pSearchTable != NULL)
            STORM_FREE(hs->pSearchTable);
        STORM_FREE(hs);
        hs = NULL;
    }
}

//-----------------------------------------------------------------------------
// Public functions

HANDLE WINAPI SFileFindFirstFile(HANDLE hMpq, const char * szMask, SFILE_FIND_DATA * lpFindFileData, const char * szListFile)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    TMPQSearch * hs = NULL;
    size_t nSize  = 0;
    int nError = ERROR_SUCCESS;

    // Check for the valid parameters
    if(!IsValidMpqHandle(hMpq))
        nError = ERROR_INVALID_HANDLE;
    if(szMask == NULL || lpFindFileData == NULL)
        nError = ERROR_INVALID_PARAMETER;

    // Include the listfile into the MPQ's internal listfile
    // Note that if the listfile name is NULL, do nothing because the
    // internal listfile is always included.
    if(nError == ERROR_SUCCESS && szListFile != NULL && *szListFile != 0)
        nError = SFileAddListFile((HANDLE)ha, szListFile);

    // Allocate the structure for MPQ search
    if(nError == ERROR_SUCCESS)
    {
        nSize = sizeof(TMPQSearch) + strlen(szMask) + 1;
        if((hs = (TMPQSearch *)STORM_ALLOC(char, nSize)) == NULL)
            nError = ERROR_NOT_ENOUGH_MEMORY;
    }

    // Perform the first search
    if(nError == ERROR_SUCCESS)
    {
        memset(hs, 0, sizeof(TMPQSearch));
        strcpy(hs->szSearchMask, szMask);
        hs->dwFlagMask = MPQ_FILE_EXISTS;
        hs->ha = ha;

        // If the archive is patched archive, we have to create a merge table
        // to prevent files being repeated
        if(ha->haPatch != NULL)
        {
            hs->dwSearchTableItems = GetSearchTableItems(ha);
            hs->pSearchTable = STORM_ALLOC(TFileEntry *, hs->dwSearchTableItems);
            hs->dwFlagMask = MPQ_FILE_EXISTS | MPQ_FILE_PATCH_FILE;
            if(hs->pSearchTable != NULL)
                memset(hs->pSearchTable, 0, hs->dwSearchTableItems * sizeof(TFileEntry *));
            else
                nError = ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    // Perform first item searching
    if(nError == ERROR_SUCCESS)
    {
        nError = DoMPQSearch(hs, lpFindFileData);
    }

    // Cleanup
    if(nError != ERROR_SUCCESS)
    {
        FreeMPQSearch(hs);
        SetLastError(nError);
    }
    
    // Return the result value
    return (HANDLE)hs;
}

bool WINAPI SFileFindNextFile(HANDLE hFind, SFILE_FIND_DATA * lpFindFileData)
{
    TMPQSearch * hs = IsValidSearchHandle(hFind);
    int nError = ERROR_SUCCESS;

    // Check the parameters
    if(hs == NULL)
        nError = ERROR_INVALID_HANDLE;
    if(lpFindFileData == NULL)
        nError = ERROR_INVALID_PARAMETER;

    if(nError == ERROR_SUCCESS)
        nError = DoMPQSearch(hs, lpFindFileData);

    if(nError != ERROR_SUCCESS)
        SetLastError(nError);
    return (nError == ERROR_SUCCESS);
}

bool WINAPI SFileFindClose(HANDLE hFind)
{
    TMPQSearch * hs = IsValidSearchHandle(hFind);

    // Check the parameters
    if(hs == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return false;
    }

    FreeMPQSearch(hs);
    return true;
}
