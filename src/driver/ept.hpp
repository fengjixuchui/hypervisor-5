#pragma once

#define DECLSPEC_PAGE_ALIGN DECLSPEC_ALIGN(PAGE_SIZE)

namespace vmx
{
	using pml4 = ept_pml4;
	using pml3 = epdpte;
	using pml2 = epde_2mb;
	using pml2_ptr = epde;
	using pml1 = epte;

	struct ept_split
	{
		DECLSPEC_PAGE_ALIGN pml1 pml1[EPT_PTE_ENTRY_COUNT]{};

		union
		{
			pml2 entry{};
			pml2_ptr pointer;
		};

		ept_split* next_split{nullptr};
	};


	struct ept_hook
	{
		DECLSPEC_PAGE_ALIGN uint8_t fake_page[PAGE_SIZE]{};

		uint64_t physical_base_address{};

		pml1* target_page{};
		pml1 original_entry{};
		pml1 shadow_entry{};
		pml1 hooked_entry{};

		uint8_t* trampoline{nullptr};
		ept_hook* next_hook{nullptr};
	};

	struct guest_context;

	class ept
	{
	public:
		ept();
		~ept();

		ept(ept&&) = delete;
		ept(const ept&) = delete;
		ept& operator=(ept&&) = delete;
		ept& operator=(const ept&) = delete;

		void initialize();

		void install_hook(PVOID TargetFunction, PVOID HookFunction, PVOID* OrigFunction);
		void handle_violation(guest_context& guest_context) const;

		pml4* get_pml4();
		const pml4* get_pml4() const;

	private:
		DECLSPEC_PAGE_ALIGN pml4 epml4[EPT_PML4E_ENTRY_COUNT]{};
		DECLSPEC_PAGE_ALIGN pml3 epdpt[EPT_PDPTE_ENTRY_COUNT]{};
		DECLSPEC_PAGE_ALIGN pml2 epde[EPT_PDPTE_ENTRY_COUNT][EPT_PDE_ENTRY_COUNT]{};

		ept_split* ept_splits{nullptr};
		ept_hook* ept_hooks{nullptr};

		pml2* get_pml2_entry(uint64_t physical_address);
		pml1* get_pml1_entry(uint64_t physical_address);
		pml1* find_pml1_table(uint64_t physical_address) const;

		ept_split* allocate_ept_split();
		ept_hook* allocate_ept_hook();

		void split_large_page(uint64_t physical_address);
	};
}