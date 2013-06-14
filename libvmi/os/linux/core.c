/* The LibVMI Library is an introspection library that simplifies access to 
 * memory in a target virtual machine or in a file containing a dump of 
 * a system's physical memory.  LibVMI is based on the XenAccess Library.
 *
 * Copyright 2011 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
 * retains certain rights in this software.
 *
 * Author: Bryan D. Payne (bdpayne@acm.org)
 * Author: Steve Maresca (steve@zentific.com)
 *
 * This file is part of LibVMI.
 *
 * LibVMI is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * LibVMI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with LibVMI.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libvmi.h"
#include "private.h"
#include "driver/interface.h"

status_t
linux_init(
    vmi_instance_t vmi)
{
    status_t ret = VMI_FAILURE;

    if (vmi->cr3) {
        vmi->kpgd = vmi->cr3;
    }
    else if (VMI_SUCCESS ==
             linux_system_map_symbol_to_address(vmi, "swapper_pg_dir",
                                                &vmi->kpgd)) {
        dbprint("--got vaddr for swapper_pg_dir (0x%.16"PRIx64").\n",
                vmi->kpgd);
        if (driver_is_pv(vmi)) {
            vmi->kpgd = vmi_translate_kv2p(vmi, vmi->kpgd);
            if (vmi_read_addr_pa(vmi, vmi->kpgd, &(vmi->kpgd)) ==
                VMI_FAILURE) {
                errprint("Failed to get physical addr for kpgd.\n");
                goto _exit;
            }
        }
        else {
            vmi->kpgd = vmi_translate_kv2p(vmi, vmi->kpgd);
        }
    }
    else {
        errprint("swapper_pg_dir not found and CR3 not set, exiting\n");
        goto _exit;
    }

    vmi->kpgd = vmi->cr3;
    dbprint("**set vmi->kpgd (0x%.16"PRIx64").\n", vmi->kpgd);

    vmi->init_task = vmi_translate_ksym2v(vmi, "init_task");

    ret = VMI_SUCCESS;

_exit:
    return ret;
}

/* helper function. Linux-specific pa to va, no page handling required
 *  (only for kernel mode and only for use during early library initialization)
 *
 * assumptions about 32b versus 64b rely upon callers only providing
 *  kernel addresses, as this function is too simplistic to ever produce
 *  correct results for userspace virtuial addresses. (intentional)
 *
 * the constants below have been stable across many kernel versions.
 *  phys_base is rarely non-zero (but this needs some verification).
 */
#define START_KERNEL_map 0xffffffff80000000
#define phys_base 0x0
static inline addr_t __phys_addr(vmi_instance_t vmi, addr_t x) {
    if (x >= START_KERNEL_map) {
        /* 64b */
        x -= START_KERNEL_map;
        x += phys_base;
    } else {
        /* 32b */
        x -= vmi->page_offset;
    }
    return x;
}

addr_t linux_find_cr3 (vmi_instance_t vmi) {
   
    addr_t cr3 = 0; 
    addr_t vpgd = 0;
    addr_t pgd_offset = vmi->os.linux_instance.pgd_offset;
    addr_t init_mm_vaddr = vmi_translate_ksym2v(vmi, "init_mm");

    int is64 = (init_mm_vaddr & 0xf000000000000000) == 0xf000000000000000;

    if(is64){
        vmi_read_64_pa(vmi, __phys_addr(vmi, init_mm_vaddr+pgd_offset), &vpgd);
        vmi->page_mode = VMI_PM_IA32E;
        cr3 = __phys_addr(vmi, vpgd);
    } else {
        vmi_read_32_pa(vmi, __phys_addr(vmi, init_mm_vaddr+pgd_offset), &vpgd);
        dbprint("-- pgd is (v)%lx==(p)%lx\n", vpgd, __phys_addr(vmi, vpgd));
        //vmi->page_mode = VMI_PM_LEGACY;
        vmi->page_mode = VMI_PM_PAE;
        cr3 = __phys_addr(vmi, vpgd);
    }

    /* TODO test page modes here to validate translation */
    //    dbprint("-- init_mm (v)%lx==(p)%lx vpgd %lx pgd %lx\n", init_mm_vaddr, __phys_addr(vmi, init_mm_vaddr), vpgd, __phys_addr(vmi, vpgd));
    return cr3;

}
