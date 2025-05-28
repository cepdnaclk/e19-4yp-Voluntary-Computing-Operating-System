org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A

jmp short start
nop

bdb_oem:                    db 'MSWIN4.1' ; 8 byte
bdb_bytes_per_sector:       dw 512
bdb_bytes_per_cluster:      db 1
bdb_reserved_sectors:       dw 1
bdb_fat_count:              db 2
bdb_dir_entries_count:      dw 0E0h
bdb__total_sectors:         dw 2880 ; 2880* 512 = 1.44
bdb_media_descriptor_type:  db 0F0h
bdb_sector_per_fat:         dw 9
bdb_sectors_per_track:      dw 18
bdb_heads:                  dw 2
bdb_hidden_sectors:         dd 0
bdb_large_sector_count:     dd 0

erb_drive_number:           db 0
                            db 0
ebr_signature:              db 29h
ebr_volume_id:              db 12h, 34h, 56h, 78h
ebr_volume_lableL           db 'FOXJELLY OS'
ebr_system_id:              db 'FAT12   '

start:
    jmp main

puts:
    push si
    push ax
    push bx

.loop:
    lodsb
    or al, al
    jz .done

    mov ah, 0x0E
    mov bh, 0
    int 0x10

    jmp .loop

.done:
    pop bx
    pop ax
    pop si
    ret

main:
    ; hlt
    mov ax, 0
    mov ds, ax
    mov es, ax

    mov ss, ax
    mov sp, 0x7C00

    mov [erb_drive_number], dl

    mov ax, 1 ; LBA=1, second sector from disk. This is your LBA value (sector 1)
    mov cl, 1 ; Number of sectors to read
    mov bx, 0x7E00 ; Where to load the data
    call disk_read

    mov si, msg_hello
    call puts

    cli
    hlt

floppy_error:
    mov si, msg_read_failed
    call puts
    jmp wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0
    int 16h
    jmp 0FFFFh:0 ; jump to begining of BIOS

.hlt:
    cli ; disable interupt so hlt
    hlt

lba_to_chs:
    push ax
    push dx

    xor dx, dx ; dx = 0
    div word [bdb_sectors_per_track] ; ax = LBA / Sectors per Track
                                     ; dx = LBA % Sectors per Track
    
    inc dx ; increment
    mov cx, dx

    xor dx, dx
    div word [bdb_heads] ; ax = (LBA / Sectors per Track ) / Heads
                         ; dx = (LBA / Sectors per Track ) % Heads

    mov dh, dl
    mov ch, al
    shl ah, 6 ; 1100
    or cl, ah

    pop ax
    mov dl, al
    pop ax
    ret 

disk_read:
    push ax
    push bx
    push cx
    push dx
    push di

    push cx
    call lba_to_chs
    pop ax

    mov ah, 02h
    mov di, 3

.retry:
    pusha ; save all registers
    stc ; set carry flag
    int 13h ; carry flag cleared
    jnc .done

    ; read failed, if carry is set
    popa
    call disk_reset

    dec di
    test di,di
    jnz .retry

.fail:
    jmp floppy_error

.done:
    popa

    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

disk_reset:
    pusha
    mov ah, 0
    stc
    int 13h
    jc floppy_error
    popa
    ret

msg_hello: db 'Hello Fox! Bootloader', ENDL, 0
msg_read_failed: db 'Read from disk failed!', ENDL, 0

times 510- ($-$$)db 0
dw 0AA55h