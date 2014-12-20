namespace Rw {

struct PS2InstanceData
{
	uint32 noRefChain;
	uint32 dataSize;
	uint8 *data;
	Material *material;
}

struct PS2InstanceDataHeader : InstanceDataHeader
{
	uint32 numMeshes;
	PS2InstanceData *instanceMeshes;
};

}
