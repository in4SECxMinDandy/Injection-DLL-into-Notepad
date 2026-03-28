# Notepad++ Process Injection & Security Research Project

Dß╗▒ ├ín n├áy l├á mß╗Öt phi├¬n bß║ún nghi├¬n cß╗⌐u ─æß║╖c biß╗çt dß╗▒a tr├¬n m├ú nguß╗ôn cß╗ºa Notepad++, tß║¡p trung v├áo viß╗çc hiß╗çn thß╗▒c h├│a v├á ph├ón t├¡ch c├íc kß╗╣ thuß║¡t **Process Injection** n├óng cao v├á **─Éiß╗üu khiß╗ân luß╗ông thß╗▒c thi (Execution Flow Control)** trong m├┤i tr╞░ß╗¥ng Windows.

## ≡ƒÄ» Mß╗Ñc ti├¬u dß╗▒ ├ín

Mß╗Ñc ti├¬u ch├¡nh l├á trß║ú lß╗¥i c├óu hß╗Åi: **L├ám thß║┐ n├áo ─æß╗â thß╗▒c thi mß╗Öt payload (DLL/Shellcode) ho├án tß║Ñt tr╞░ß╗¢c khi giao diß╗çn ng╞░ß╗¥i d├╣ng (GUI) cß╗ºa ch╞░╞íng tr├¼nh gß╗æc hiß╗ân thß╗ï?**

Dß╗▒ ├ín bao gß╗ôm c├íc th├ánh phß║ºn nghi├¬n cß╗⌐u vß╗ü:

- Kiß╗âm so├ít thß╗⌐ tß╗▒ nß║íp module cß╗ºa Windows Loader.
- C├íc kß╗╣ thuß║¡t ch├¿n m├ú (Injection) tß╗½ mß╗⌐c ─æß╗Ö c╞í bß║ún ─æß║┐n n├óng cao.
- ─Éß╗ông bß╗Ö h├│a giß╗»a tiß║┐n tr├¼nh "kß║╗ tß║Ñn c├┤ng" v├á "nß║ín nh├ón" ─æß╗â ─æiß╗üu khiß╗ân timeline thß╗▒c thi.

## ≡ƒÜÇ C├íc kß╗╣ thuß║¡t trß╗ìng t├óm

Dß╗▒ ├ín triß╗ân khai v├á ph├ón t├¡ch 3 kß╗╣ thuß║¡t ch├¡nh:

1. **DLL Injection**: Sß╗¡ dß╗Ñng `CreateRemoteThread` v├á `LoadLibrary` ─æß╗â nß║íp module ngoß║íi vi v├áo kh├┤ng gian ─æß╗ïa chß╗ë cß╗ºa Notepad++.
2. **Process Hollowing (RunPE)**: Tß║ío tiß║┐n tr├¼nh trong trß║íng th├íi `SUSPENDED`, giß║úi ph├│ng bß╗Ö nhß╗¢ gß╗æc (`Unmap`) v├á thay thß║┐ bß║▒ng payload t├╣y chß╗ënh.
3. **Thread Execution Hijacking**: Chiß║┐m quyß╗ün ─æiß╗üu khiß╗ân cß╗ºa mß╗Öt luß╗ông ─æang chß║íy (`SuspendThread` + `SetThreadContext`) ─æß╗â ─æiß╗üu h╞░ß╗¢ng thß╗▒c thi.

## ≡ƒôé Cß║Ñu tr├║c t├ái liß╗çu & C├┤ng cß╗Ñ

- **[Process_Injection_Report.md](Process_Injection_Report.md)**: B├ío c├ío kß╗╣ thuß║¡t chi tiß║┐t (h╞ín 1000 d├▓ng) ph├ón t├¡ch s├óu vß╗ü c╞í chß║┐ nß║íp cß╗ºa Windows, s╞í ─æß╗ô Timeline Analysis v├á c├íc ─æoß║ín m├ú mß║½u (C++, Python).
- **[cpu_stress.py](cpu_stress.py)**: Mß╗Öt kß╗ïch bß║ún Python m├┤ phß╗Ång h├ánh vi "Malware" thß╗▒c tß║┐:
  - G├óy qu├í tß║úi CPU/RAM.
  - V├┤ hiß╗çu h├│a n├║t ─æ├│ng Console (`SC_CLOSE`).
  - Giao tiß║┐p vß╗¢i Command & Control (C2) qua Telegram API.
  - C╞í chß║┐ kh├│a/mß╗ƒ bß║▒ng mß║¡t m├ú (`UNLOCK_CODE`).
- **PowerEditor/src/**: M├ú nguß╗ôn l├╡i cß╗ºa Notepad++ ─æ╞░ß╗úc sß╗¡ dß╗Ñng ─æß╗â x├íc ─æß╗ïnh c├íc "─æiß╗âm ch├¿n" (Injection Points) l├╜ t╞░ß╗ƒng nh╞░ TLS Callbacks hoß║╖c tr╞░ß╗¢c v├▓ng lß║╖p WinMain.

## ≡ƒ¢á Kß╗╣ thuß║¡t ─æiß╗üu khiß╗ân luß╗ông (Execution Control)

─Éß╗â ─æß║úm bß║úo payload chß║íy tr╞░ß╗¢c GUI, dß╗▒ ├ín ├íp dß╗Ñng:

- **CREATE_SUSPENDED**: Khß╗ƒi tß║ío tiß║┐n tr├¼nh nh╞░ng giß╗» luß╗ông ch├¡nh ß╗ƒ trß║íng th├íi chß╗¥.
- **Wait Synchronization**: Sß╗¡ dß╗Ñng **Mutex** hoß║╖c **Event Objects** to├án cß╗Ñc ─æß╗â tiß║┐n tr├¼nh gß╗æc chß╗ë hiß╗ân thß╗ï cß╗¡a sß╗ò sau khi payload gß╗¡i t├¡n hiß╗çu "─æ├ú xong".
- **Entry Point Hijacking**: Thay ─æß╗òi ─æß╗ïa chß╗ë thß╗▒c thi tß║íi PEB (Process Environment Block) ─æß╗â ╞░u ti├¬n payload.

## ΓÜá∩╕Å Cß║únh b├ío bß║úo mß║¡t (Disclaimer)

Dß╗▒ ├ín n├áy ─æ╞░ß╗úc thß╗▒c hiß╗çn vß╗¢i mß╗Ñc ─æ├¡ch **nghi├¬n cß╗⌐u v├á gi├ío dß╗Ñc (Educational Purposes Only)**.

- Kh├┤ng sß╗¡ dß╗Ñng c├íc kß╗╣ thuß║¡t n├áy cho mß╗Ñc ─æ├¡ch bß║Ñt hß╗úp ph├íp hoß║╖c tß║Ñn c├┤ng m├íy t├¡nh kh├┤ng ─æ╞░ß╗úc ph├⌐p.
- T├íc giß║ú kh├┤ng chß╗ïu tr├ích nhiß╗çm vß╗ü bß║Ñt kß╗│ thiß╗çt hß║íi n├áo g├óy ra bß╗ƒi viß╗çc sß╗¡ dß╗Ñng sai mß╗Ñc ─æ├¡ch c├íc th├┤ng tin trong kho l╞░u trß╗» n├áy.

## ≡ƒô£ License

Dß╗▒ ├ín dß╗▒a tr├¬n m├ú nguß╗ôn cß╗ºa [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus) v├á tu├ón thß╗º giß║Ñy ph├⌐p **GPL-3.0 License**.

---
*Nghi├¬n cß╗⌐u ─æ╞░ß╗úc thß╗▒c hiß╗çn bß╗ƒi ─æß╗Öi ng┼⌐ in4SECxMinDandy.*
