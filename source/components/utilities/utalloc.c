
/******************************************************************************
 * 
 * Module Name: cmalloc - local memory allocation routines
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (without the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions 
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.  
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee 
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.  
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE. 
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#define __CMALLOC_C__

#include <acpi.h>
#include <acpiobj.h>
#include <interpreter.h>
#include <namespace.h>
#include <acpiosd.h>


#define _THIS_MODULE        "cmalloc.c"
#define _COMPONENT          MISCELLANEOUS

/*
 * Most of this code is for tracking memory leaks in the subsystem, and it
 * gets compiled out when the ACPI_DEBUG flag is not set.  It works like so:
 * every memory allocation is kept track of in a doubly linked list.  Each
 * element contains the caller's component, module name, function name, and
 * line number.  _CmAllocate and _CmCallocate call CmAddElementToAllocList
 * to add an element to the list; deletion occurs in the bosy of _CmFree.
 */

#ifndef ACPI_DEBUG

#define CmAddElementToAllocList
#define CmDelteElementFromAllocList
#define CmDumpCurrentAllocations

#else

enum {
	MALLOC = 0,
	CALLOC
};

typedef struct ALLOCATION_INFO {
	void					*Address;
	UINT32					Size;
	UINT8					AllocType;
	UINT32 					Component;
	char					Module[32];
	INT32					Line;
	struct ALLOCATION_INFO	*Previous;
	struct ALLOCATION_INFO	*Next;
} ALLOCATION_INFO;

ALLOCATION_INFO *HeadAllocPtr;
ALLOCATION_INFO *TailAllocPtr;

/*****************************************************************************
 * 
 * FUNCTION:    CmAddElementToAllocList
 *
 * PARAMETERS:  Address				Address of allocated memory
 *              Size				Size of the allocation
 *              AllocType			MALLOC or CALLOC
 *              Component			Component type of caller
 *              Module				Source file name of caller
 *              Line				Line number of caller
 *              Function			Calling function name
 *
 * RETURN:      
 *
 * DESCRIPTION: Inserts an element into the global allocation tracking list.
 *
 ****************************************************************************/

void
CmAddElementToAllocList (
	void 					*Address,
	UINT32					Size,
	UINT8					AllocType,
	UINT32					Component,
	ACPI_STRING				Module,
	INT32                   Line)
{
	FUNCTION_TRACE ("CmAddElementToAllocList");
	
	/* Any list locking should be done right here. */
	
	/* If the head pointer is null, create the first element and fill it in. */
	if (NULL == HeadAllocPtr)
	{
		HeadAllocPtr = (ALLOCATION_INFO *) OsdCallocate (sizeof (ALLOCATION_INFO));
		
		/* error check */
		
		TailAllocPtr = HeadAllocPtr;
	}
	else
	{
		TailAllocPtr->Next = (ALLOCATION_INFO *) OsdCallocate (sizeof (ALLOCATION_INFO));
		
		/* error check */
		
		TailAllocPtr->Next->Previous = TailAllocPtr;
		TailAllocPtr = TailAllocPtr->Next;
	}

	/* Fill in the instance data. */		
	TailAllocPtr->Address = Address;
	TailAllocPtr->Size = Size;
	TailAllocPtr->AllocType = AllocType;
	TailAllocPtr->Component = Component;
	strcpy (TailAllocPtr->Module, Module);
	TailAllocPtr->Line = Line;
	
	FUNCTION_EXIT;
	return;
}


/*****************************************************************************
 * 
 * FUNCTION:    CmDeleteElementFromAllocList
 *
 * PARAMETERS:  Address				Address of allocated memory
 *              Size				Size of the allocation
 *              AllocType			MALLOC or CALLOC
 *              Component			Component type of caller
 *              Module				Source file name of caller
 *              Line				Line number of caller
 *              Function			Calling function name
 *
 * RETURN:      
 *
 * DESCRIPTION: Inserts an element into the global allocation tracking list.
 *
 ****************************************************************************/

void
CmDeleteElementFromAllocList (
	void 					*Address,
	UINT32					Component,
	ACPI_STRING				Module,
	INT32                   Line)
{
	ALLOCATION_INFO 	*Element = HeadAllocPtr;
	
	FUNCTION_TRACE ("CmDeleteElementFromAllocList");

	/* cases: none, one, multiple. */
	if (NULL == HeadAllocPtr)
	{
		/* Boy we got problems. */
		_REPORT_ERROR (Module, Line, Component,
       		"CmDeleteElementFromAllocList: Empty allocation list and someone's calling CmFree.");
		
		FUNCTION_EXIT;
		return;
	}
	
	if (HeadAllocPtr == TailAllocPtr)
	{	
		if (Address != HeadAllocPtr->Address)
		{
			_REPORT_ERROR (Module, Line, Component,
				"CmDeleteElementFromAllocList: Deleting non-allocated memory...");
			FUNCTION_EXIT;
			return;
		}
		
		OsdFree (HeadAllocPtr);
		HeadAllocPtr = NULL;
		TailAllocPtr = NULL;
		
		DEBUG_PRINT (TRACE_ALLOCATIONS,
			("_CmFree: Allocation list deleted.  No more outstanding allocations.\n"));
	
		FUNCTION_EXIT;
		return;
	}
	
	/* search and destroy. note - this always searches the entire list...*/
	for (;;)
	{		
		if (Element->Address == Address)
		{
			/* cases: head, tail, other */
			if (Element == HeadAllocPtr)
			{
				Element->Next->Previous = NULL;
				HeadAllocPtr = Element->Next;
			}
			else
			{
				if (Element == TailAllocPtr)
				{
					Element->Previous->Next = NULL;
					TailAllocPtr = Element->Previous;
				}
				else
				{
					Element->Previous->Next = Element->Next;
					Element->Next->Previous = Element->Previous;
				}
			}		
			
			OsdFree (Element);
			FUNCTION_EXIT;
			return;
		}
			
		if (Element->Next == NULL)
		{
			_REPORT_ERROR (Module, Line, Component,
				"_CmFree: Reached the end of the list without finding the entry.");
			FUNCTION_EXIT;
			return;
		}
		
		Element = Element->Next;
		
	}
	
	FUNCTION_EXIT;
}


/*****************************************************************************
 * 
 * FUNCTION:    CmDumpCurrentAllocations
 *
 * PARAMETERS:  Component			Componet(s) to dump info for.
 *              Module				Module to dump info for.  NULL means all.
 *
 * RETURN:      
 *
 * DESCRIPTION: 
 *
 ****************************************************************************/

void
CmDumpCurrentAllocations (
	UINT32					Component,
	ACPI_STRING				Module)
{
	ALLOCATION_INFO		*Element = HeadAllocPtr;
	
	FUNCTION_TRACE ("CmDumpCurrentAllocations");
	
	if (Element == NULL)
	{
		DEBUG_PRINT (TRACE_ALLOCATIONS, ("No outstanding allocations.\n"));
		FUNCTION_EXIT;
		return;
	}
	
	for (;;)
	{
		if ((Element->Component & Component) &&
			((Module == NULL) || (0 == strcmp (Module, Element->Module))))
		{
			DEBUG_PRINT (TRACE_ALLOCATIONS,
				("%08x bytes at %p from file %s (line %d)\n",
				Element->Size, Element->Address, Element->Module, Element->Line));
		}
		
		if (Element->Next == NULL)
		{
			FUNCTION_EXIT;
			return;
		}
		
		Element = Element->Next;
	}

	/* won't ever get here. */
}	


#endif


/*****************************************************************************
 * 
 * FUNCTION:    _CmAllocate
 *
 * PARAMETERS:  Size				Size of the allocation
 *              Component			Component type of caller
 *              Module				Source file name of caller
 *              Line				Line number of caller
 *              Function			Calling function name
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: The subsystem's equivalent of malloc.
 *
 ****************************************************************************/

void *
_CmAllocate (
	UINT32					Size,
	UINT32					Component,
	ACPI_STRING				Module,
	INT32                   Line)
{
	void *Address = NULL;

	FUNCTION_TRACE ("_CmAllocate");
	
	Address = OsdAllocate (Size);
	
    if (!Address)
    {
       	/* Report allocation error */
       	_REPORT_ERROR (Module, Line, Component,
       		"CmAllocate: Memory allocation failure");
    }
    else
    {
        DEBUG_PRINT (TRACE_ALLOCATIONS, ("CmAllocate: %x Size 0x%x\n",
        	Address, Size));
    }
    
    CmAddElementToAllocList (Address, Size, MALLOC, Component, Module, Line);

	FUNCTION_EXIT;	
	return Address;
}


/*****************************************************************************
 * 
 * FUNCTION:    _CmCallocate
 *
 * PARAMETERS:  Size				Size of the allocation
 *              Component			Component type of caller
 *              Module				Source file name of caller
 *              Line				Line number of caller
 *              Function			Calling function name
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: Subsystem equivalent of calloc.
 *
 ****************************************************************************/

void *
_CmCallocate (
	UINT32					Size,
	UINT32					Component,
	ACPI_STRING				Module,
	INT32                   Line)
{
	void *Address = NULL;

	FUNCTION_TRACE ("_CmCallocate");
		
	Address = OsdCallocate (Size);
	
    if (!Address)
    {
       	/* Report allocation error */
       	_REPORT_ERROR (Module, Line, Component,
       		"CmCallocate: Memory allocation failure");
    }
    else
    {
        DEBUG_PRINT (TRACE_ALLOCATIONS, ("CmCallocate: %x Size 0x%x\n",
        	Address, Size));
    }

    CmAddElementToAllocList (Address, Size, CALLOC, Component, Module, Line);
	
	FUNCTION_EXIT;
	return Address;
}


/*****************************************************************************
 * 
 * FUNCTION:    _CmFree
 *
 * PARAMETERS:  Address				Address of the memory to deallocate
 *              Component			Component type of caller
 *              Module				Source file name of caller
 *              Line				Line number of caller
 *              Function			Calling function name
 *
 * RETURN:      
 *
 * DESCRIPTION: Frees the memory at Address
 *
 ****************************************************************************/

void
_CmFree (
	void					*Address,
	UINT32					Component,
	ACPI_STRING				Module,
	INT32                   Line)
{
	FUNCTION_TRACE ("_CmFree");
	
	if (NULL == Address)
	{
		_REPORT_ERROR (Module, Line, Component,
    		"_CmFree: Trying to delete a NULL address.");
    	FUNCTION_EXIT;
    	return;
    
    }	
	
	CmDeleteElementFromAllocList (Address, Component, Module, Line);
	OsdFree (Address);
	
	FUNCTION_EXIT;
}


/*****************************************************************************
 * 
 * FUNCTION:    _AllocateObjectDesc
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      Pointer to newly allocated object descriptor.  Null on error
 *
 * DESCRIPTION: Allocate a new object descriptor.  Gracefully handle
 *              error conditions.
 *
 ****************************************************************************/

void *
_AllocateObjectDesc (
    char                    *ModuleName, 
    INT32                   LineNumber, 
    INT32                   ComponentId)
{
    ACPI_OBJECT_INTERNAL    *NewDesc;
    
    FUNCTION_TRACE ("_AllocateObjectDesc");

    /* Attempt to allocate new descriptor */

    NewDesc = _CmCallocate (sizeof (ACPI_OBJECT_INTERNAL),
    	ComponentId,
    	ModuleName,
    	LineNumber);
    	
    if (!NewDesc)
    {
        /* Allocation failed */
        
        _REPORT_ERROR (ModuleName, LineNumber, ComponentId, 
                        "Could not allocate Object Descriptor");
    }

    else
    {
        DEBUG_PRINT (TRACE_ALLOCATIONS, ("AllocateObjectDesc: %x Size 0x%x\n",
                        NewDesc, sizeof (ACPI_OBJECT_INTERNAL)));
    }

	FUNCTION_EXIT;
    return NewDesc;
}



/*****************************************************************************
 * 
 * FUNCTION:    LocalDeleteObject
 *
 * PARAMETERS:  **ObjDesc           - Descriptor to be deleted
 *
 * DESCRIPTION: If the passed descriptor pointer does not point into the
 *              AML block and is not an NsHandle, free the descriptor.
 *
 *              Note that the parameter is the address of the descriptor
 *              pointer, so that the descriptor pointer can be set to NULL
 *              after the descriptor is freed.
 *
 ****************************************************************************/

void
LocalDeleteObject (
    ACPI_OBJECT_INTERNAL    **ObjDesc)
{

    FUNCTION_TRACE ("LocalDeleteObject");
    DEBUG_PRINT (ACPI_INFO, ("LocalDeleteObject: Obj %x at %p\n", *ObjDesc, ObjDesc));


    /*
     * Be very careful about what we delete
     */

    /* 
     * XXX: This is not the best solution!
     * XXX: And may not work in all cases!!
     */


    if ((ACPI_OBJECT_INTERNAL **) 0 !=    ObjDesc  &&
        (ACPI_OBJECT_INTERNAL *) 0 !=    *ObjDesc  &&
        !AmlIsInPCodeBlock ((UINT8 *)    *ObjDesc) &&
        !IS_NS_HANDLE                   (*ObjDesc) &&
        !AmlIsMethodValue               (*ObjDesc) &&
        !IsNsValue                      (*ObjDesc))
    {

        DEBUG_PRINT (ACPI_INFO, ("LocalDeleteObject: Actually deleting %x\n", *ObjDesc));

        CmFree (*ObjDesc);

        DEBUG_PRINT (ACPI_INFO, ("LocalDeleteObject: Successfully deleted %x\n", *ObjDesc));

    }

    /* In all cases, set the pointer to null */

    *ObjDesc = NULL;
    FUNCTION_EXIT;
}
