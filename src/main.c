#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "typecheck.h"
#include "environment.h"
#include "interpreter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <source_file> [--dump-ast] [--dump-tokens] [--export-web]\n",prog);
    exit(1);
}

static char *read_file(const char *filename) {
    FILE *f=fopen(filename,"rb");
    if(!f){fprintf(stderr,"Error: cannot open '%s'\n",filename);exit(1);}
    fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
    char *buf=(char*)malloc((size_t)len+1);
    fread(buf,1,(size_t)len,f); buf[len]='\0'; fclose(f);
    return buf;
}

static void dump_tokens(const TokenList *tl) {
    printf("%-15s | %-25s | %s\n","TOKEN TYPE","VALUE","LINE");
    printf("---------------------------------------------------\n");
    for(size_t i=0;i<tl->count;i++){
        const Token *t=&tl->tokens[i];
        char disp[36];
        if(t->type==TOKEN_COMMENT&&strlen(t->value)>30) snprintf(disp,sizeof(disp),"%.27s...",t->value);
        else snprintf(disp,sizeof(disp),"%s",t->value);
        printf("(%-13s, '%-23s', %d)\n",token_type_name(t->type),disp,t->line_number);
    }
    printf("\n");
}

/* ── JSON export (unchanged from before) ── */
static void json_str(FILE *f,const char *s){
    fputc('"',f);
    for(;*s;s++){
        if(*s=='"')fputs("\\\"",f);
        else if(*s=='\\')fputs("\\\\",f);
        else if(*s=='\n')fputs("\\n",f);
        else fputc(*s,f);
    }
    fputc('"',f);
}
static void export_props(FILE *f,ASTNode *n){
    fputc('[',f);
    for(int i=0;i<n->data.resource.prop_count;i++){
        ASTNode *p=n->data.resource.props[i];
        if(p->type!=NODE_PROP){
            fprintf(f,"{\"type\":\"card\",\"name\":");
            json_str(f,p->data.resource.name);
            fputs(",\"props\":",f); export_props(f,p); fputc('}',f);
        } else {
            fputs("{\"key\":",f); json_str(f,p->data.prop.key);
            if(p->data.prop.is_int) fprintf(f,",\"value\":%d}",p->data.prop.val_int);
            else if(p->data.prop.is_bool) fprintf(f,",\"value\":%s}",p->data.prop.bool_val?"true":"false");
            else{fputs(",\"value\":",f);json_str(f,p->data.prop.val_str);fputc('}',f);}
        }
        if(i<n->data.resource.prop_count-1) fputc(',',f);
    }
    fputc(']',f);
}
static void write_game_json(FILE *f,ASTNode *root){
    fputs("{\n",f);
    bool first;

    fputs("  \"resources\": [",f); first=true;
    for(int i=0;i<root->data.program.count;i++){ASTNode *d=root->data.program.decls[i];
        if(d->type!=NODE_RESOURCE) continue;
        if(!first)fputc(',',f);first=false;
        fputs("{\"name\":",f);json_str(f,d->data.resource.name);fputs(",\"props\":",f);export_props(f,d);fputc('}',f);}
    fputs("],\n",f);

    fputs("  \"arenas\": [",f); first=true;
    for(int i=0;i<root->data.program.count;i++){ASTNode *d=root->data.program.decls[i];
        if(d->type!=NODE_ARENA) continue;
        if(!first)fputc(',',f);first=false;
        fputs("{\"name\":",f);json_str(f,d->data.resource.name);fputs(",\"props\":",f);export_props(f,d);fputc('}',f);}
    fputs("],\n",f);

    fputs("  \"tiles\": [",f); first=true;
    for(int i=0;i<root->data.program.count;i++){ASTNode *d=root->data.program.decls[i];
        if(d->type!=NODE_TILE) continue;
        if(!first)fputc(',',f);first=false;
        fputs("{\"name\":",f);json_str(f,d->data.resource.name);fputs(",\"props\":",f);export_props(f,d);fputc('}',f);}
    fputs("],\n",f);

    fputs("  \"decks\": [",f); first=true;
    for(int i=0;i<root->data.program.count;i++){ASTNode *d=root->data.program.decls[i];
        if(d->type!=NODE_DECK) continue;
        if(!first)fputc(',',f);first=false;
        fputs("{\"name\":",f);json_str(f,d->data.resource.name);fputs(",\"cards\":",f);export_props(f,d);fputc('}',f);}
    fputs("],\n",f);

    fputs("  \"players\": {\"count\":2}",f);
    for(int i=0;i<root->data.program.count;i++){ASTNode *d=root->data.program.decls[i];
        if(d->type!=NODE_PLAYERS) continue;
        /* overwrite */
        fseek(f,-1,SEEK_CUR); fputs("",f);
        for(int j=0;j<d->data.resource.prop_count;j++){
            ASTNode *p=d->data.resource.props[j];
            if(p->type!=NODE_PROP) continue;
            if(j) fputc(',',f);
            json_str(f,p->data.prop.key); fputc(':',f);
            if(p->data.prop.is_int) fprintf(f,"%d",p->data.prop.val_int);
            else{json_str(f,p->data.prop.val_str);}
        }
        fputc('}',f); break;
    }
    fputs(",\n",f);

    fputs("  \"phases\": [",f); first=true;
    for(int i=0;i<root->data.program.count;i++){ASTNode *d=root->data.program.decls[i];
        if(d->type!=NODE_TURN_STRUCT) continue;
        for(int j=0;j<d->data.turn_struct.count;j++){
            if(!first)fputc(',',f);first=false;
            json_str(f,d->data.turn_struct.phases[j]->data.phase.name);
        }}
    fputs("],\n",f);

    fputs("  \"events\": [",f); first=true;
    for(int i=0;i<root->data.program.count;i++){ASTNode *d=root->data.program.decls[i];
        if(d->type!=NODE_EVENT) continue;
        if(!first)fputc(',',f);first=false;
        fputs("{\"on\":",f);json_str(f,d->data.event.event_type);
        fputs(",\"params\":[",f);
        for(int j=0;j<d->data.event.param_count;j++){if(j)fputc(',',f);json_str(f,d->data.event.params[j]);}
        fputs("]}",f);}
    fputs("]\n}\n",f);
}

int main(int argc, char **argv) {
    srand((unsigned)time(NULL));
    setvbuf(stdout,NULL,_IONBF,0);
    if(argc<2) usage(argv[0]);

    const char *filename=argv[1];
    bool do_ast=false, do_tokens=false, do_web=false;
    for(int i=2;i<argc;i++){
        if(strcmp(argv[i],"--dump-ast")==0)    do_ast=true;
        else if(strcmp(argv[i],"--dump-tokens")==0) do_tokens=true;
        else if(strcmp(argv[i],"--export-web")==0)  do_web=true;
        else{fprintf(stderr,"Unknown flag: %s\n",argv[i]);usage(argv[0]);}
    }

    char      *source=read_file(filename);
    TokenList  tokens=tokenize(source);
    free(source);

    if(do_tokens){
        printf("=== TOKEN DUMP (%zu tokens) ===\n",tokens.count);
        dump_tokens(&tokens);
    }

    ASTNode *root=parse_program(&tokens);

    printf("=== SEMANTIC ANALYSIS ===\n");
    typecheck_program(root);
    printf("OK: no type errors.\n\n");

    printf("=== INTERPRETER ===\n");

    /* Global scope — lives for the whole program */
    push_scope();

    for(int i=0;i<root->data.program.count;i++){
        ASTNode *d=root->data.program.decls[i];

        if(d->type==NODE_EVENT){
            printf("\n>>> Event: on %s\n",d->data.event.event_type);

            /*
             * Turn scope for event-only dry-run.
             * In a real game loop this would be shared with
             * the phase scope that ran before the event.
             */
            interpreter_begin_turn();

            /* Seed built-in player variables */
            interpreter_seed_player(
                /*id*/1, /*hp*/100, /*money*/1500, /*pos*/0
            );

            /* Inject declared params */
            for(int p=0;p<d->data.event.param_count;p++){
                Value mv; mv.type=VAL_INT; mv.data.i_val=(p+1)*100;
                declare_var(d->data.event.params[p],VAL_INT,mv);
                printf("    param %s = %d\n",d->data.event.params[p],mv.data.i_val);
            }

            printf("    -> Executing...\n");
            exec_stmt(d->data.event.body);

            interpreter_end_turn();
            printf("<<< done\n");

        } else if(d->type==NODE_TURN_STRUCT){
            printf("\n>>> Turn structure (%d phases)\n",d->data.turn_struct.count);

            /*
             * One turn scope wraps ALL phases — variables assigned
             * in phase N are visible in phase N+1 and in events.
             */
            interpreter_begin_turn();
            interpreter_seed_player(1,100,1500,0);

            for(int j=0;j<d->data.turn_struct.count;j++){
                ASTNode *ph=d->data.turn_struct.phases[j];
                printf("  >> Phase: %s\n",ph->data.phase.name);
                exec_phase_body(ph->data.phase.body);
            }

            /* Show final built-in state */
            Value *phv=get_var("player_hp");
            Value *pmv=get_var("player_money");
            if(phv&&pmv)
                printf("  -- Player state: HP=%d  Gold=%d\n",phv->data.i_val,pmv->data.i_val);

            interpreter_end_turn();
            printf("<<< turn done\n");
        }
    }

    pop_scope();
    printf("\n=== EXECUTION COMPLETE ===\n\n");

    if(do_ast){ printf("=== AST ===\n"); ast_dump(root,0); }
    else {
        printf("Parse+Typecheck+Eval OK — %d decl(s) in '%s'\n"
               "Flags: --dump-ast  --dump-tokens  --export-web\n",
               root->data.program.count, filename);
    }

    if(do_web){
        FILE *jf=fopen("game_data.json","w");
        if(!jf){fprintf(stderr,"Error: cannot write game_data.json\n");}
        else{ write_game_json(jf,root); fclose(jf); printf("Exported: game_data.json\n"); }
    }

    ast_free(root);
    free_token_list(&tokens);
    return 0;
}