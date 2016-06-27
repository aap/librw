namespace rw {
namespace gl3 {

void matfxRenderCB(Atomic *atomic, InstanceDataHeader *header);

ObjPipeline *makeSkinPipeline(void);
ObjPipeline *makeMatFXPipeline(void);

void initMatFX(void);
void initSkin(void);

}
}
