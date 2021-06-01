#include<types.h>
#include<mmap.h>

// Helper function to create a new vm_area
struct vm_area* create_vm_area(u64 start_addr, u64 end_addr, u32 flags, u32 mapping_type)
{
	struct vm_area *new_vm_area = alloc_vm_area();
	new_vm_area-> vm_start = start_addr;
	new_vm_area-> vm_end = end_addr;
	new_vm_area-> access_flags = flags;
	new_vm_area->mapping_type = mapping_type;
	return new_vm_area;
}


int normal_pagefault(struct exec_context *current, u64 addr, int error_code){
	// printk("Handling normal pagefault!")
	// get base addr of pgdir
	u64 *vaddr_base = (u64 *)osmap(current->pgd);
	u64 *entry;
	u64 pfn;
	// set User and Present flags
	// set Write flag if specified in error_code
	u64 ac_flags = 0x5 | (error_code & 0x2);
	
	// find the entry in page directory
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PUD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PMD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	if(*entry & 0x1) {
		// PMD->PTE Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PLD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
	// since this fault occured as frame was not present, we don't need present check here
	pfn = os_pfn_alloc(USER_REG);
	*entry = (pfn << PTE_SHIFT) | ac_flags;

	return 1;
}

int hugepg_pagefault(struct exec_context *current, u64 addr, int error_code){
	// get base addr of pgdir
	u64 *vaddr_base = (u64 *)osmap(current->pgd);
	u64 *entry;
	u64 pfn;
	// set User and Present flags
	// set Write flag if specified in error_code
	u64 ac_flags = 0x5 | (error_code & 0x2);
	
	// find the entry in page directory
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PUD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PMD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	
	// since this fault occured as huge page frame was not present, we don't need present check here
	u32 pfn_hpg = get_hugepage_pfn(os_hugepage_alloc());
	*entry = (pfn << PTE_SHIFT) | (ac_flags|0x80);

	return 1;
}


/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid access. Map the physical page 
 * Return 0
 * 
 * For invalid access, i.e Access which is not matching the vm_area access rights (Writing on ReadOnly pages)
 * return -1. 
 */
int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
	// printk("PAGE FAULT!\nADDR: %x\nERROR_CODE : %x\n\n", addr, error_code);
	if(error_code == 0x7)
		return -1;

	struct vm_area* vm_node = current->vm_area;
	while(vm_node){
		if(vm_node->vm_start <= addr && vm_node->vm_end > addr)
			break;
		vm_node = vm_node->vm_next;
	}
	if(vm_node==NULL)
		return -1;

	if(vm_node->access_flags==PROT_READ && error_code==0x6)
		return -1;

	if(vm_node->mapping_type==NORMAL_PAGE_MAPPING){
		return normal_pagefault(current, addr, error_code);
	}else{
		return hugepg_pagefault(current, addr, error_code);
	}
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
	if(current==NULL)
		return -1;
	
	// if this is the first mmap call, add the dummy vm_area
	if(current->vm_area==NULL){
		struct vm_area* dummy_area = create_vm_area(MMAP_AREA_START, MMAP_AREA_START + 0x1000, 0x4, NORMAL_PAGE_MAPPING);
		
		current->vm_area = dummy_area;
	}

	int num_vm_areas = length/0x1000;
	if(length%0x1000)
		num_vm_areas++;
	
	if(flags==MAP_FIXED){
		if((u64*)addr==NULL || addr%0x1000!=0 || addr+num_vm_areas*0x1000>MMAP_AREA_END)
			return -1;

		struct vm_area* vm_node1 = current->vm_area;
		struct vm_area* vm_node2 = NULL;
	
		while(vm_node1){
			if(vm_node1->vm_start > addr)
				break;
			vm_node2 = vm_node1;
			vm_node1 = vm_node1->vm_next;
		}

		if(vm_node1==NULL){
			if(vm_node2->vm_end > addr)
				return -1;
			else if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node2->vm_end== addr && vm_node2->access_flags==prot){
				vm_node2->vm_end += 0x1000 * num_vm_areas;
				return vm_node2->vm_end;
			}else{
				struct vm_area* new_vm_area = create_vm_area(addr, addr + 0x1000*num_vm_areas, prot, NORMAL_PAGE_MAPPING);
				new_vm_area->vm_next = NULL;

				vm_node2->vm_next = new_vm_area;
				return new_vm_area->vm_start;
			}
		}else{
			if(vm_node2->vm_end > addr || vm_node1->vm_start < addr + 0x1000*num_vm_areas){
				return -1;
			}else if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node1->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node2->vm_end == addr && vm_node1->vm_start == addr + 0x1000*num_vm_areas && vm_node1->access_flags==prot && vm_node2->access_flags==prot){
				vm_node2->vm_next = vm_node1->vm_next;
				vm_node1->vm_next = NULL;
				vm_node2->vm_end = vm_node1->vm_end;
				dealloc_vm_area(vm_node1);
				return addr;
			}else if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node2->vm_end == addr && vm_node2->access_flags==prot){
				vm_node2->vm_end += 0x1000*num_vm_areas;
				return vm_node2->vm_end;
			}else if(vm_node1->mapping_type!=HUGE_PAGE_MAPPING && vm_node1->vm_end == addr && vm_node1->access_flags==prot){
				vm_node1->vm_start = addr;
				return vm_node1->vm_start;
			}else{
				struct vm_area* new_vm_area = create_vm_area(addr, addr + 0x1000*num_vm_areas, prot, NORMAL_PAGE_MAPPING);
			
				vm_node2->vm_next = new_vm_area;
				new_vm_area->vm_next = vm_node1;
				return new_vm_area->vm_start;
			}
		}
	}else{
		if(addr%0x1000)
			addr = (addr - addr%0x1000) + 0x1000;
		
		if((u64*)addr == NULL){
			// printk("ADDR IS NULL!\n");
			struct vm_area* vm_node1 = current->vm_area;
			struct vm_area* vm_node2 = NULL;
			u64 curr_addr = vm_node1->vm_end;

			while(vm_node1){
				if(vm_node1->vm_start >= curr_addr + num_vm_areas*0x1000)
					break;
				curr_addr = vm_node1->vm_end;
				vm_node2 = vm_node1;
				vm_node1 = vm_node1->vm_next;
			}
			// printk("VM NODE 1 BEGIN : %x\nCURR_ADDR : %x\nCURR_END_ADDR : %x\n", vm_node1->vm_start, curr_addr, curr_addr + 0x1000*num_vm_areas);
			// printk("VM NODE 1 ACCESS FLAGS: %x\nPROT : %x\n", vm_node1->access_flags, prot);
			if(vm_node1==NULL){
				if(vm_node2->vm_end > curr_addr)
					return -1;
				else if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node2->vm_end== curr_addr && vm_node2->access_flags==prot){
					vm_node2->vm_end += 0x1000 * num_vm_areas;
					return vm_node2->vm_end;
				}else{
					struct vm_area* new_vm_area = create_vm_area(curr_addr, curr_addr + 0x1000*num_vm_areas, prot, NORMAL_PAGE_MAPPING);
					new_vm_area->vm_next = NULL;

					vm_node2->vm_next = new_vm_area;
					return new_vm_area->vm_start;
				}
			}else{
				
				if(curr_addr + num_vm_areas*0x1000 > MMAP_AREA_END)
					return -1;
				if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node1->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node1->vm_start == curr_addr + 0x1000*num_vm_areas && vm_node1->access_flags==prot && vm_node2->access_flags==prot){
					vm_node2->vm_next = vm_node1->vm_next;
					vm_node1->vm_next = NULL;
					vm_node2->vm_end = vm_node1->vm_end;
					dealloc_vm_area(vm_node1);
					return curr_addr;
				}else if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node2->access_flags==prot){
					vm_node2->vm_end += 0x1000*num_vm_areas;
					return curr_addr;
				}else if(vm_node1->mapping_type!=HUGE_PAGE_MAPPING && vm_node1->vm_start == curr_addr + 0x1000*num_vm_areas && vm_node1->access_flags==prot){
					vm_node1->vm_start = curr_addr;
					return curr_addr;
				}else{
					struct vm_area* new_vm_area = create_vm_area(curr_addr, curr_addr+0x1000*num_vm_areas, prot, NORMAL_PAGE_MAPPING);
					
					vm_node2->vm_next = new_vm_area;
					new_vm_area->vm_next = vm_node1;
					return new_vm_area->vm_start;
				}
			}
		}else{
			struct vm_area* vm_node1 = current->vm_area;
			struct vm_area* vm_node2 = NULL;
			
			while(vm_node1){
				if(vm_node1->vm_start > addr)
					break;
				vm_node2 = vm_node1;
				vm_node1 = vm_node1->vm_next;
			}
			// printk("VM NODE 1 START : %x END : %x\n", vm_node1->vm_start, vm_node1->vm_end);
			if(vm_node1==NULL){
				if(vm_node2->vm_end > addr)
					return -1;
				else if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node2->vm_end == addr && vm_node2->access_flags==prot){
					vm_node2->vm_end += 0x1000 * num_vm_areas;
					return vm_node2->vm_end;
				}else{
					struct vm_area* new_vm_area = create_vm_area(addr, addr + 0x1000*num_vm_areas, prot, NORMAL_PAGE_MAPPING);
					new_vm_area->vm_next = NULL;

					vm_node2->vm_next = new_vm_area;
					return new_vm_area->vm_start;
				}
			}else{
				if(vm_node2->vm_end > addr || vm_node1->vm_start < addr + 0x1000*num_vm_areas){
					return vm_area_map(current, (u64)NULL, length, prot, flags);
				}else if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node1->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node2->vm_end == addr && vm_node1->vm_start == addr + 0x1000*num_vm_areas && vm_node1->access_flags==prot && vm_node2->access_flags==prot){
					vm_node2->vm_next = vm_node1->vm_next;
					vm_node1->vm_next = NULL;
					vm_node2->vm_end = vm_node1->vm_end;
					dealloc_vm_area(vm_node1);
					return addr;
				}else if(vm_node2->mapping_type!=HUGE_PAGE_MAPPING && vm_node2->vm_start!=MMAP_AREA_START && vm_node2->vm_end == addr && vm_node2->access_flags==prot){
					vm_node2->vm_end += 0x1000*num_vm_areas;
					return addr;
				}else if(vm_node1->mapping_type!=HUGE_PAGE_MAPPING && vm_node1->vm_end == addr + 0x1000*num_vm_areas && vm_node1->access_flags==prot){
					vm_node1->vm_start = addr;
					return addr;
				}else{
					struct vm_area* new_vm_area = create_vm_area(addr, addr + 0x1000*num_vm_areas, prot, NORMAL_PAGE_MAPPING);
					
					vm_node2->vm_next = new_vm_area;
					new_vm_area->vm_next = vm_node1;
					return addr;
				}
			}
		}
	}
}

void unmap_normal_vm_area(struct exec_context* current, u64 start_addr, u64 end_addr, struct vm_area** vm_node1, struct vm_area** vm_node2){
	u64 start_unmap;
	u64 end_unmap;
	// printk("\nVM NODE VM_START : %x , VM_END : %x\n", (*vm_node1)->vm_start, (*vm_node1)->vm_end);
	if(start_addr <= (*vm_node1)->vm_start && end_addr >= (*vm_node1)->vm_end){
		start_unmap = (*vm_node1)->vm_start;
		end_unmap = (*vm_node1)->vm_end;
		// printk("1.Start Unmap : %x\nEnd Unmap : %x\n", start_unmap, end_unmap);

		(*vm_node2)->vm_next = (*vm_node1)->vm_next;
		(*vm_node1)->vm_next = NULL;
		dealloc_vm_area(*vm_node1);
		*vm_node1 = *vm_node2;
	}else if(start_addr >= (*vm_node1)->vm_start && start_addr < (*vm_node1)->vm_end && end_addr >= (*vm_node1)->vm_end){
		start_unmap = start_addr;
		end_unmap = (*vm_node1)->vm_end;
		// printk("2.Start Unmap : %x\nEnd Unmap : %x\n", start_unmap, end_unmap);
	
		(*vm_node1)->vm_end = start_addr;
	}else if(start_addr <= (*vm_node1)->vm_start && end_addr <= (*vm_node1)->vm_end && end_addr > (*vm_node1)->vm_start){
		start_unmap = (*vm_node1)->vm_start;
		end_unmap = end_addr;
		// printk("3.Start Unmap : %x\nEnd Unmap : %x\n", start_unmap, end_unmap);
			
		(*vm_node1)->vm_start = end_addr;
	}else if(start_addr > (*vm_node1)->vm_start && end_addr < (*vm_node1)->vm_end){
		start_unmap = start_addr;
		end_unmap = end_addr;
		// printk("4.Start Unmap : %x\nEnd Unmap : %x\n", start_unmap, end_unmap);
			
		struct vm_area* new_vm_area = create_vm_area((*vm_node1)->vm_start, start_addr, (*vm_node1)->access_flags, (*vm_node1)->mapping_type);
		new_vm_area->vm_next = (*vm_node1);

		(*vm_node2)->vm_next = new_vm_area;
		(*vm_node1)->vm_start = end_addr;
	}else{
		return;
	}

	for(u64 unmap_addr = start_unmap; unmap_addr<end_unmap; unmap_addr+=0x1000){
			
		int check_pmd = 0;
		int check_pud = 0;
		// get base addr of pgdir
		u64 *vaddr_base_pgd = (u64 *)osmap(current->pgd);
		u64 *entry_pgd;

		// find the entry in page directory
		entry_pgd = (u64*)(vaddr_base_pgd + ((unmap_addr & PGD_MASK) >> PGD_SHIFT));
		if(*entry_pgd & 0x1) {
			// PGD->PUD Present, access it
			u64 pfn_pud = (*entry_pgd >> PTE_SHIFT) & 0xFFFFFFFF;
			u64 vaddr_base_pud = (u64 *)osmap(pfn_pud);
			u64 *entry_pud = (u64*)(vaddr_base_pud + ((unmap_addr & PUD_MASK) >> PUD_SHIFT));
			if(*entry_pud & 0x1) {
				// PUD->PMD Present, access it
				u64 pfn_pmd = (*entry_pud >> PTE_SHIFT) & 0xFFFFFFFF;
				u64 vaddr_base_pmd = (u64 *)osmap(pfn_pmd);

				u64 *entry_pmd = vaddr_base_pmd + ((unmap_addr & PMD_MASK) >> PMD_SHIFT);
				if(*entry_pmd & 0x1) {
					// PMD->PTE Present, access it
					u64 pfn_pte = (*entry_pmd >> PTE_SHIFT) & 0xFFFFFFFF;
					u64 vaddr_base_pte = (u64 *)osmap(pfn_pte);

					u64 *entry_pte = vaddr_base_pte + ((unmap_addr & PTE_MASK) >> PTE_SHIFT);
					if(*entry_pte & 0x1){
						u64 pfn_phys = (*entry_pte >> PTE_SHIFT) & 0xFFFFFFFF;
						os_pfn_free(USER_REG, pfn_phys);

						// invalidates tlb entry corresponding to Virtual Address addr 
						asm volatile (
							"invlpg (%0);" 
							:: "r"(unmap_addr) 
							: "memory"
						);

						*entry_pte = *entry_pte^0x1;
					}
					entry_pte = vaddr_base_pte;
					while(entry_pte<vaddr_base_pte + 0x1000 && !*entry_pte&0x1){
						entry_pte += 0x40;
					}
					if(entry_pte==vaddr_base_pte + 0x1000){
						os_pfn_free(OS_PT_REG, pfn_pte);
						*entry_pmd = *entry_pmd^0x1;
						check_pmd=1;
					}

				}
				if(check_pmd){
					entry_pmd = vaddr_base_pmd;
					while(entry_pmd<vaddr_base_pmd + 0x1000 && !*entry_pmd&0x1){
						entry_pmd += 0x40;
					}
					if(entry_pmd==vaddr_base_pmd + 0x1000){
						os_pfn_free(OS_PT_REG, pfn_pmd);
						*entry_pud = *entry_pud^0x1;
						check_pud=1;
					}
				}
			}
			if(check_pud){
				entry_pud = vaddr_base_pud;
				while((u64)entry_pud < vaddr_base_pud + 0x1000 && !*entry_pud&0x1){
					entry_pud += 0x40;
				}
				if((u64)entry_pud == vaddr_base_pud + 0x1000){
					os_pfn_free(OS_PT_REG, pfn_pud);
					*entry_pgd = *entry_pgd^0x1;
				}
			}
		}
	}
}

void unmap_hpg_vm_area(struct exec_context* current, u64 start_addr, u64 end_addr, struct vm_area** vm_node1, struct vm_area** vm_node2){
	
	start_addr = start_addr - start_addr%0x200000;
	
	if(end_addr%0x200000){
		end_addr = end_addr - end_addr%0x200000 + 0x200000;
	}
	
	u64 start_unmap;
	u64 end_unmap;
	// printk("\nVM NODE VM_START : %x , VM_END : %x\n", vm_node1->vm_start, vm_node1->vm_end);
	if(start_addr <= (*vm_node1)->vm_start && end_addr >= (*vm_node1)->vm_end){
		start_unmap = (*vm_node1)->vm_start;
		end_unmap = (*vm_node1)->vm_end;
		// printk("1.Start Unmap : %x\nEnd Unmap : %x\n", start_unmap, end_unmap);

		(*vm_node2)->vm_next = (*vm_node1)->vm_next;
		(*vm_node1)->vm_next = NULL;
		dealloc_vm_area(*vm_node1);
		*vm_node1 = *vm_node2;
	}else if(start_addr >= (*vm_node1)->vm_start && start_addr < (*vm_node1)->vm_end && end_addr >= (*vm_node1)->vm_end){
		start_unmap = start_addr;
		end_unmap = (*vm_node1)->vm_end;
		// printk("2.Start Unmap : %x\nEnd Unmap : %x\n", start_unmap, end_unmap);
	
		(*vm_node1)->vm_end = start_addr;
	}else if(start_addr <= (*vm_node1)->vm_start && end_addr <= (*vm_node1)->vm_end && end_addr > (*vm_node1)->vm_start){
		start_unmap = (*vm_node1)->vm_start;
		end_unmap = end_addr;
		// printk("3.Start Unmap : %x\nEnd Unmap : %x\n", start_unmap, end_unmap);
			
		(*vm_node1)->vm_start = end_addr;
	}else if(start_addr > (*vm_node1)->vm_start && end_addr < (*vm_node1)->vm_end){
		start_unmap = start_addr;
		end_unmap = end_addr;
		// printk("4.Start Unmap : %x\nEnd Unmap : %x\n", start_unmap, end_unmap);
			
		struct vm_area* new_vm_area = create_vm_area((*vm_node1)->vm_start, start_addr, (*vm_node1)->access_flags, (*vm_node1)->mapping_type);
		new_vm_area->vm_next = *vm_node1;

		(*vm_node2)->vm_next = new_vm_area;
		(*vm_node1)->vm_start = end_addr;
	}else{
		return;
	}

	for(u64 unmap_addr = start_unmap; unmap_addr<end_unmap; unmap_addr+=0x200000){
		
		// invalidates tlb entry corresponding to Virtual Address addr 
		asm volatile (
			"invlpg (%0);" 
			:: "r"(unmap_addr) 
			: "memory"
		);	
		
		int check_pud = 0;
		// get base addr of pgdir
		u64 *vaddr_base_pgd = (u64 *)osmap(current->pgd);
		u64 *entry_pgd;

		// find the entry in page directory
		entry_pgd = (u64*)(vaddr_base_pgd + ((unmap_addr & PGD_MASK) >> PGD_SHIFT));
		if(*entry_pgd & 0x1) {
			// PGD->PUD Present, access it
			u64 pfn_pud = (*entry_pgd >> PTE_SHIFT) & 0xFFFFFFFF;
			u64 vaddr_base_pud = (u64 *)osmap(pfn_pud);
			u64 *entry_pud = (u64*)(vaddr_base_pud + ((unmap_addr & PUD_MASK) >> PUD_SHIFT));
			if(*entry_pud & 0x1) {
				// PUD->PMD Present, access it
				u64 pfn_pmd = (*entry_pud >> PTE_SHIFT) & 0xFFFFFFFF;
				u64 vaddr_base_pmd = (u64 *)osmap(pfn_pmd);

				u64 *entry_pmd = vaddr_base_pmd + ((unmap_addr & PMD_MASK) >> PMD_SHIFT);
				if(*entry_pmd & 0x1) {
					u64 pfn_hpg = (*entry_pmd >>  PTE_SHIFT) & 0xFFFFFFFF;
					os_hugepage_free(pfn_hpg*HUGE_PAGE_SIZE);

					*entry_pmd = *entry_pmd^1;
				}
				entry_pmd = vaddr_base_pmd;
				while(entry_pmd<vaddr_base_pmd + 0x1000 && !((*entry_pmd)&0x1)){
					entry_pmd += 0x40;
				}
				if(entry_pmd==vaddr_base_pmd + 0x1000){
					os_pfn_free(OS_PT_REG, pfn_pmd);
					*entry_pud = *entry_pud^0x1;
					check_pud=1;
				}
			}
			if(check_pud){
				entry_pud = vaddr_base_pud;
				while((u64)entry_pud < vaddr_base_pud + 0x1000 && !((*entry_pud)&0x1)){
					entry_pud += 0x40;
				}
				if((u64)entry_pud == vaddr_base_pud + 0x1000){
					os_pfn_free(OS_PT_REG, pfn_pud);
					*entry_pgd = *entry_pgd^0x1;
				}
			}
		}
	}
}

/**
 * munmap system call implemenations
 */
int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
	struct vm_area* vm_node1 = current->vm_area;
	struct vm_area* vm_node2 = NULL;
	
	u64 start_addr = addr - addr%0x1000;
	u64 end_addr = addr + length;
	
	if(end_addr%0x1000){
		end_addr = end_addr - end_addr%0x1000 + 0x1000;
	}

	u64 start_unmap;
	u64 end_unmap;
	
	// printk("\nUNMAP CALLED!~~~~~~~~~~~~~~~~\nStart Addr : %x\nEnd Addr : %x\n", start_addr, end_addr);
	while(vm_node1){
		
		if(vm_node1->mapping_type==NORMAL_PAGE_MAPPING)
			unmap_normal_vm_area(current, start_addr, end_addr, &vm_node1, &vm_node2);
		else
			unmap_hpg_vm_area(current, start_addr, end_addr, &vm_node1, &vm_node2);

		vm_node2 = vm_node1;
		vm_node1 = vm_node1->vm_next;
	}
	return 0;
}

/**
 *Helper function to split normal vm area at huge page boundaries
 */
void split_vma_at_boundaries(struct vm_area *vm_area, u64 hpg_start, u64 hpg_end){
	struct vm_area* vm_node1 = vm_area;
	struct vm_area* vm_node2 = NULL;
	while(vm_node1){
		if(vm_node1->vm_start < hpg_start && vm_node1->vm_end > hpg_end){
			struct vm_area* new_vm_area1 = create_vm_area(vm_node1->vm_start, hpg_start, vm_node1->access_flags, NORMAL_PAGE_MAPPING);
			struct vm_area* new_vm_area2 = create_vm_area(hpg_start, hpg_end, vm_node1->access_flags, NORMAL_PAGE_MAPPING);
			
			vm_node1->vm_start = hpg_end;

			vm_node2->vm_next = new_vm_area1;
			new_vm_area1->vm_next = new_vm_area2;
			new_vm_area2->vm_next = vm_node1;
		}else if(vm_node1->vm_start < hpg_start && vm_node1->vm_end > hpg_start){
			struct vm_area* new_vm_area = create_vm_area(vm_node1->vm_start, hpg_start, vm_node1->access_flags, NORMAL_PAGE_MAPPING);
			
			vm_node1->vm_start = hpg_start;

			vm_node2->vm_next = new_vm_area;
			new_vm_area->vm_next = vm_node1;
		}else if(vm_node1->vm_end < hpg_end && vm_node1->vm_end > hpg_end){
			struct vm_area* new_vm_area = create_vm_area(vm_node1->vm_start, hpg_end, vm_node1->access_flags, NORMAL_PAGE_MAPPING);
			
			vm_node1->vm_start = hpg_end;
			
			vm_node2->vm_next = new_vm_area;
			new_vm_area->vm_next = vm_node1;
		}else if(vm_node1->vm_start >= hpg_end){
			break;
		}
		vm_node2 = vm_node1;
		vm_node1 = vm_node1->vm_next;
	}
}


/*
Insert the created hugepage vm area and delete the normal vm areas
*/

void insert_hugepage_vma(struct vm_area* vm_area, struct vm_area* new_huge_page, u64 hpg_start, u64 hpg_end){

	struct vm_area* vm_node1 = vm_area;
	struct vm_area* vm_node2 = NULL;
	while(vm_node1){
		if(vm_node1->vm_start == hpg_start){
			vm_node2->vm_next = new_huge_page;
			new_huge_page->vm_next = vm_node1->vm_next;
			vm_node1->vm_next = NULL;
			dealloc_vm_area(vm_node1);
			vm_node1 = new_huge_page;
		}else if(vm_node1->vm_start > hpg_start && vm_node1->vm_end < hpg_end){
			vm_node2->vm_next = vm_node1->vm_next;
			vm_node1->vm_next = NULL;
			dealloc_vm_area(vm_node1);
			vm_node1 = vm_node2;
		}else if(vm_node1->vm_end == hpg_end){
			vm_node2->vm_next = vm_node1->vm_next;
			vm_node1->vm_next = NULL;
			dealloc_vm_area(vm_node1);
			break;
		}
		vm_node2 = vm_node1;
		vm_node1 = vm_node1->vm_next;
	}
}

/*
Copy data from given normal page to hugepage
*/
void copy_to_hugepage(struct exec_context* current, u64* pfn_hugepg, u64* entry_pmd, u64 vaddr_base_phys, u64 addr){
	if(*entry_pmd & 0x80){
		u64 vaddr_base_hugepg = (u64*)((*pfn_hugepg) * HUGE_PAGE_SIZE);
		u64 vaddr_base_page = vaddr_base_hugepg + (addr & 0x1FFFFF);
		// printk("Created hpg PFN and Copying!\n");
		memcpy(vaddr_base_page, vaddr_base_phys, 0x1000);
	}else{
		*pfn_hugepg = get_hugepage_pfn(os_hugepage_alloc());
		*entry_pmd = (*entry_pmd)|0x80;
		u64 vaddr_base_hugepg = (u64*)((*pfn_hugepg)*HUGE_PAGE_SIZE);
		u64 vaddr_base_page = vaddr_base_hugepg + (addr & 0x1FFFFF);
		// printk("Copying!\n");
		memcpy(vaddr_base_page, vaddr_base_phys, 0x1000);
	}
}


/*
Frees normal pages and copies the data from given 
address to the hugepage physical memory
*/

void free_and_copy_to_hugepage(struct exec_context* current, u64 hpg_start, u64 hpg_end, u32 prot){
	
	u64	pfn_hugepg;
	u64 unmap_addr = hpg_start;
	for(u64 addr = hpg_start; addr<hpg_end; addr+=0x1000){
		// invalidates tlb entry corresponding to Virtual Address addr 
		asm volatile (
			"invlpg (%0);" 
			:: "r"(addr) 
			: "memory"
		);
	}

	// get base addr of pgdir
	u64 *vaddr_base_pgd = (u64 *)osmap(current->pgd);
	u64 *entry_pgd;
	// find the entry in page directory
	entry_pgd = (u64*)(vaddr_base_pgd + ((unmap_addr & PGD_MASK) >> PGD_SHIFT));
	if(*entry_pgd & 0x1) {
		*entry_pgd = (*entry_pgd)|(0x2 & prot);

		// PGD->PUD Present, access it
		u64 pfn_pud = (*entry_pgd >> PTE_SHIFT) & 0xFFFFFFFF;
		u64 vaddr_base_pud = (u64 *)osmap(pfn_pud);
		u64 *entry_pud = (u64*)(vaddr_base_pud + ((unmap_addr & PUD_MASK) >> PUD_SHIFT));
		if(*entry_pud & 0x1) {
			*entry_pud = (*entry_pud)|(0x2 & prot);

			// PUD->PMD Present, access it
			u64 pfn_pmd = (*entry_pud >> PTE_SHIFT) & 0xFFFFFFFF;
			u64 vaddr_base_pmd = (u64 *)osmap(pfn_pmd);

			u64 *entry_pmd = vaddr_base_pmd + ((unmap_addr & PMD_MASK) >> PMD_SHIFT);
			if(*entry_pmd & 0x1) {
				*entry_pmd = (*entry_pmd)|(0x2 & prot);

				// PMD->PTE Present, access it
				u64 pfn_pte = (*entry_pmd >> PTE_SHIFT) & 0xFFFFFFFF;
				u64 vaddr_base_pte = (u64 *)osmap(pfn_pte);

				for(unmap_addr = hpg_start; unmap_addr<hpg_end; unmap_addr+=0x1000){
					
					u64 *entry_pte = vaddr_base_pte + ((unmap_addr & PTE_MASK) >> PTE_SHIFT);
					if(*entry_pte & 0x1){
						u64 pfn_phys = (*entry_pte >> PTE_SHIFT) & 0xFFFFFFFF;
						u64 vaddr_base_data = (u64 *)osmap(pfn_phys);

						copy_to_hugepage(current, &pfn_hugepg, entry_pmd,vaddr_base_data, unmap_addr);
								
						os_pfn_free(USER_REG, pfn_phys);
					}
				}
				os_pfn_free(OS_PT_REG, pfn_pte);
				*entry_pmd = (*entry_pmd & 0xFFFFF00000000FFF)|(( pfn_hugepg & 0xFFFFFFFF)<<12);
			}
		}
	}

}

/**
 * make_hugepage system call implemenation
 */
long vm_area_make_hugepage(struct exec_context *current, void *addr, u32 length, u32 prot, u32 force_prot)
{
	u64 hpg_start = (u64)addr;
	u64 hpg_end = hpg_start + length;

	if(hpg_start%0x200000)
		hpg_start = hpg_start - hpg_start%0x200000 + 0x200000;

	if(hpg_end%0x200000)
		hpg_end = hpg_end - hpg_end%0x200000;
	
	// printk("HUGE PAGE CREATE REQUEST,\nHPG_START = %x\nHPG_END = %x\n", hpg_start, hpg_end);
	if(hpg_start >= hpg_end)
		return -EINVAL;

	struct vm_area* vm_node = current->vm_area;
	u64 addr_ptr = hpg_start;
	
	while(vm_node){
		// printk("ADDR PTR : %x\n", addr_ptr);
		if(addr_ptr >= hpg_end)
			break;
		
		if(vm_node->vm_start <= addr_ptr && vm_node->vm_end <= hpg_end && vm_node->vm_end > addr_ptr){
			if(vm_node->mapping_type == HUGE_PAGE_MAPPING){
				// printk("VMA OCCUPIED\n");
				return -EVMAOCCUPIED;
			}
			// printk("PROT = %x ACCESS_FLAGS = %x\n", prot, vm_node->access_flags);
			if(!force_prot && vm_node->access_flags!=prot){
				// printk("DIFFPROT\n");
				return -EDIFFPROT;
			}
				
			
			addr_ptr = vm_node->vm_end;

		}else if(vm_node->vm_start > addr_ptr){
			// printk("NO MAPPING\n");
			return -ENOMAPPING;
		}
		
		vm_node = vm_node->vm_next;
	}

	// printk("CREATING HUGE PAGE AFTER ERROR CHECK!\n");
	split_vma_at_boundaries(current->vm_area, hpg_start, hpg_end);
	// pmap(1);
	// printk("SPLIT DONE!\n");
	struct vm_area* new_huge_page = create_vm_area(hpg_start, hpg_end, prot, HUGE_PAGE_MAPPING);
	
	insert_hugepage_vma(current->vm_area, new_huge_page, hpg_start, hpg_end);

	free_and_copy_to_hugepage(current, hpg_start, hpg_end, prot);

	vm_node = current->vm_area->vm_next;
	struct vm_area* vm_node_prev = current->vm_area;

	while(vm_node){
		if(vm_node_prev->vm_end == vm_node->vm_start && vm_node_prev->mapping_type == vm_node->mapping_type && vm_node_prev->access_flags == vm_node->access_flags){
			vm_node_prev->vm_end = vm_node->vm_end;
			vm_node_prev->vm_next = vm_node->vm_next;
			vm_node->vm_next = NULL;
			dealloc_vm_area(vm_node);
			vm_node = vm_node_prev;
		}
		vm_node_prev = vm_node;
		vm_node = vm_node->vm_next;
	}

	
	return hpg_start;
}


void split_hugepages_at_boundary(struct vm_area* vm_area, u64 start_addr, u64 end_addr){
	struct vm_area* vm_node1 = vm_area;
	struct vm_area* vm_node2 = NULL;
	while(vm_node1){
		if(vm_node1->mapping_type==HUGE_PAGE_MAPPING){
			if(vm_node1->vm_start < start_addr && vm_node1->vm_end > end_addr){
				struct vm_area* new_vm_area1 = create_vm_area(vm_node1->vm_start, start_addr, vm_node1->access_flags, HUGE_PAGE_MAPPING);
				struct vm_area* new_vm_area2 = create_vm_area(start_addr, end_addr, vm_node1->access_flags, HUGE_PAGE_MAPPING);
				
				vm_node1->vm_start = end_addr;

				vm_node2->vm_next = new_vm_area1;
				new_vm_area1->vm_next = new_vm_area2;
				new_vm_area2->vm_next = vm_node1;
			}else if(vm_node1->vm_start > start_addr && vm_node1->vm_end >= end_addr){
				struct vm_area* new_vm_area = create_vm_area(vm_node1->vm_start, start_addr, vm_node1->access_flags, HUGE_PAGE_MAPPING);
				
				vm_node1->vm_start = start_addr;

				vm_node2->vm_next = new_vm_area;
				new_vm_area->vm_next = vm_node1;
			}else if(vm_node1->vm_start >= start_addr && vm_node1->vm_end > end_addr){
				struct vm_area* new_vm_area = create_vm_area(vm_node1->vm_start, end_addr, vm_node1->access_flags, HUGE_PAGE_MAPPING);
				
				vm_node1->vm_start = end_addr;
				
				vm_node2->vm_next = new_vm_area;
				new_vm_area->vm_next = vm_node1;
			}else if(vm_node1->vm_start >= end_addr){
				break;
			}
		}
		vm_node2 = vm_node1;
		vm_node1 = vm_node1->vm_next;
	}
}

void copy_and_free_hugepg(struct exec_context* current, struct vm_area* vm_node){
	vm_node->mapping_type = NORMAL_PAGE_MAPPING;

	for(u64 hpg_addr = vm_node->vm_start; hpg_addr<vm_node->vm_end; hpg_addr+=0x200000){

		u64 pfn_pte =  os_pfn_alloc(OS_PT_REG);
		u64 vaddr_base_pte = (u64*)osmap(pfn_pte);

		// get base addr of pgdir
		u64 *vaddr_base_pgd = (u64 *)osmap(current->pgd);
		u64 *entry_pgd;
		// find the entry in page directory
		entry_pgd = (u64*)(vaddr_base_pgd + ((hpg_addr & PGD_MASK) >> PGD_SHIFT));
		if(*entry_pgd & 0x1) {
			// PGD->PUD Present, access it
			u64 pfn_pud = (*entry_pgd >> PTE_SHIFT) & 0xFFFFFFFF;
			u64 vaddr_base_pud = (u64 *)osmap(pfn_pud);
			u64 *entry_pud = (u64*)(vaddr_base_pud + ((hpg_addr & PUD_MASK) >> PUD_SHIFT));
			if(*entry_pud & 0x1) {
				// PUD->PMD Present, access it
				u64 pfn_pmd = (*entry_pud >> PTE_SHIFT) & 0xFFFFFFFF;
				u64 vaddr_base_pmd = (u64 *)osmap(pfn_pmd);

				u64 *entry_pmd = vaddr_base_pmd + ((hpg_addr & PMD_MASK) >> PMD_SHIFT);
				if(*entry_pmd & 0x1) {
					u64 pfn_hugepg = (*entry_pmd >> PTE_SHIFT) & 0xFFFFFFFF;
					u64 vaddr_base_hugepg = pfn_hugepg*HUGE_PAGE_SIZE;

					for(u64 normalpg_addr = hpg_addr; normalpg_addr < hpg_addr + 0x200000; normalpg_addr+=0x1000){
						u64* hpg_entry = vaddr_base_hugepg + (normalpg_addr & 0x1FFFFF);
						
						u64* entry_pte = vaddr_base_pte + (normalpg_addr & PTE_MASK) >> PTE_SHIFT;
						
						u64 pfn_normalpg = os_pfn_alloc(USER_REG);
						u64 vaddr_base_normalpg = (u64*)osmap(pfn_normalpg);
						
						*entry_pte = (pfn_normalpg << PTE_SHIFT)|(0x5 | (0x2 &(*entry_pmd)));
						memcpy(vaddr_base_normalpg, hpg_entry, 0x1000);

					}
					os_hugepage_free(vaddr_base_hugepg);
					*entry_pmd = (pfn_pte << PTE_SHIFT) | (0x5 | (0x2 &(*entry_pmd)));

				}
			}
		}
	}

}

/**
 * break_system call implemenation
 */
int vm_area_break_hugepage(struct exec_context *current, void *addr, u32 length)
{
	u64 start_addr = addr;
	u64 end_addr = addr + length;
	if(start_addr%0x200000 || end_addr%0x200000)
		return -EINVAL;
	
	split_hugepages_at_boundary(current->vm_area, start_addr, end_addr);
	// printk("Details after split\n");
	// pmap(1);
	// printk("-----------------\n");

	struct vm_area* vm_node = current->vm_area;

	while(vm_node){
		if(vm_node->mapping_type==HUGE_PAGE_MAPPING && vm_node->vm_start <= start_addr && vm_node->vm_end <= end_addr){
			copy_and_free_hugepg(current, vm_node);
		}
		vm_node = vm_node->vm_next;
	}

	vm_node = current->vm_area->vm_next;
	struct vm_area* vm_node_prev = current->vm_area;

	while(vm_node){
		if(vm_node_prev->vm_end == vm_node->vm_start && vm_node_prev->mapping_type == vm_node->mapping_type && vm_node_prev->access_flags == vm_node->access_flags){
			vm_node_prev->vm_end = vm_node->vm_end;
			vm_node_prev->vm_next = vm_node->vm_next;
			vm_node->vm_next = NULL;
			dealloc_vm_area(vm_node);
			vm_node = vm_node_prev;
		}
		vm_node_prev = vm_node;
		vm_node = vm_node->vm_next;
	}

	return 0;
}
