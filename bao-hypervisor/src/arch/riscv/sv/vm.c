#include <vm.h>
#include <mem.h>
#include <page_table.h>
#include <arch/csrs.h>

void vm_arch_mem_prot_init(struct vm* vm)
{
    paddr_t root_pt_pa;
    mem_translate(&cpu()->as, (vaddr_t)vm->as.pt.root, &root_pt_pa);

    vm->arch.hgatp = (root_pt_pa >> PAGE_SHIFT) | (HGATP_MODE_DFLT) |
        ((vm->id << HGATP_VMID_OFF) & HGATP_VMID_MSK);
}

void vcpu_arch_mem_prot_init(struct vcpu* vcpu)
{
    UNUSED_ARG(vcpu);
}

void vcpu_arch_mem_prot_reset(struct vcpu* vcpu)
{
    UNUSED_ARG(vcpu);
}

void vcpu_arch_mem_prot_save_state(struct vcpu* vcpu)
{
    UNUSED_ARG(vcpu);
}

void vcpu_arch_mem_prot_restore_state(struct vcpu* vcpu)
{
    UNUSED_ARG(vcpu);
}
