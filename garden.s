.set GARDEN_TYPE, 7

.text
.align 2

.global garden_factory_entry
.type garden_factory_entry, STT_FUNC
garden_factory_entry:
    R1 = GARDEN_TYPE (X);
    CC = R0 == R1;
    IF !CC JUMP .Lfactory_other;
    [--SP] = RETS;
    SP += -12;
    CALL garden_factory;
    SP += 12;
    RETS = [SP++];
    RTS;
.Lfactory_other:
    P1.H = garden_factory_resume;
    P1.L = garden_factory_resume;
    JUMP (P1);
.size garden_factory_entry, .-garden_factory_entry

.global garden_name_entry
.type garden_name_entry, STT_FUNC
garden_name_entry:
    [--SP] = RETS;
    [--SP] = R2;
    [--SP] = R1;
    [--SP] = R0;
    SP += -12;
    CALL garden_type_of_name;
    SP += 12;
    CC = R0 == 0;
    IF CC JUMP .Lname_other;
    SP += 12;
    RETS = [SP++];
    RTS;
.Lname_other:
    R0 = [SP++];
    R1 = [SP++];
    R2 = [SP++];
    RETS = [SP++];
    P1.H = garden_name_resume;
    P1.L = garden_name_resume;
    JUMP (P1);
.size garden_name_entry, .-garden_name_entry

.global garden_type_name_entry
.type garden_type_name_entry, STT_FUNC
garden_type_name_entry:
    R1 = GARDEN_TYPE (X);
    CC = R0 == R1;
    IF !CC JUMP .Ltype_name_other;
    R0.H = garden_type_name;
    R0.L = garden_type_name;
    RTS;
.Ltype_name_other:
    P1.H = garden_type_name_resume;
    P1.L = garden_type_name_resume;
    JUMP (P1);
.size garden_type_name_entry, .-garden_type_name_entry

.global garden_controller_entry
.type garden_controller_entry, STT_FUNC
garden_controller_entry:
    [--SP] = RETS;
    [--SP] = R2;
    [--SP] = R1;
    [--SP] = R0;
    SP += -12;
    CALL garden_controller_for_type;
    SP += 12;
    CC = R0 == 0;
    IF CC JUMP .Lcontroller_other;
    SP += 12;
    RETS = [SP++];
    RTS;
.Lcontroller_other:
    R0 = [SP++];
    R1 = [SP++];
    R2 = [SP++];
    RETS = [SP++];
    P1.H = garden_controller_resume;
    P1.L = garden_controller_resume;
    JUMP (P1);
.size garden_controller_entry, .-garden_controller_entry

.global garden_screen_controller_entry
.type garden_screen_controller_entry, STT_FUNC
garden_screen_controller_entry:
    [--SP] = RETS;
    [--SP] = R2;
    [--SP] = R1;
    [--SP] = R0;
    SP += -12;
    CALL garden_controller_for_type;
    SP += 12;
    CC = R0 == 0;
    IF CC JUMP .Lscreen_controller_other;
    SP += 12;
    RETS = [SP++];
    RTS;
.Lscreen_controller_other:
    R0 = [SP++];
    R1 = [SP++];
    R2 = [SP++];
    RETS = [SP++];
    P1.H = garden_screen_controller_resume;
    P1.L = garden_screen_controller_resume;
    JUMP (P1);
.size garden_screen_controller_entry, .-garden_screen_controller_entry

.section .bss
.balign 4
.global garden_factory_resume
.type garden_factory_resume, STT_OBJECT
garden_factory_resume:
    .space 16
.size garden_factory_resume, .-garden_factory_resume

.global garden_name_resume
.type garden_name_resume, STT_OBJECT
garden_name_resume:
    .space 16
.size garden_name_resume, .-garden_name_resume

.global garden_type_name_resume
.type garden_type_name_resume, STT_OBJECT
garden_type_name_resume:
    .space 24
.size garden_type_name_resume, .-garden_type_name_resume

.global garden_controller_resume
.type garden_controller_resume, STT_OBJECT
garden_controller_resume:
    .space 24
.size garden_controller_resume, .-garden_controller_resume

.global garden_screen_controller_resume
.type garden_screen_controller_resume, STT_OBJECT
garden_screen_controller_resume:
    .space 24
.size garden_screen_controller_resume, .-garden_screen_controller_resume
