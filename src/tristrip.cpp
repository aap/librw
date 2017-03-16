#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"

#define PLUGIN_ID 2

namespace rw {

struct GraphEdge
{
	int32 node;	/* index of the connected node */
	uint32 isConnected : 1;	/* is connected to other node */
	uint32 otherEdge : 2;	/* edge number on connected node */
	uint32 isStrip : 1;	/* is strip edge */
};

struct StripNode
{
	uint16 v[3];	/* vertex indices */
	uint8 parent : 2;	/* tunnel parent node (edge index) */
	uint8 visited : 1;	/* visited in breadth first search */
	uint8 stripVisited : 1;	/* strip starting at this node was visited during search */
	uint8 isEnd : 1;	/* is in end list */
	GraphEdge e[3];
	int32 stripId;	/* index of start node */
//		int asdf;
	LLLink inlist;
};

struct StripMesh
{
	int32 numNodes;
	StripNode *nodes;
	LinkList loneNodes;	/* nodes not connected to any others */
	LinkList endNodes;	/* strip start/end nodes */
};

static void
printNode(StripMesh *sm, StripNode *n)
{
	printf("%3ld: %3d  %3d.%d  %3d.%d %3d.%d || %3d %3d %3d\n",
		n - sm->nodes,
		n->stripId,
		n->e[0].node,
		n->e[0].isStrip,
		n->e[1].node,
		n->e[1].isStrip,
		n->e[2].node,
		n->e[2].isStrip,
		n->v[0],
		n->v[1],
		n->v[2]);
}

static void
printLone(StripMesh *sm)
{
	FORLIST(lnk, sm->loneNodes)
		printNode(sm, LLLinkGetData(lnk, StripNode, inlist));
}

static void
printEnds(StripMesh *sm)
{
	FORLIST(lnk, sm->endNodes)
		printNode(sm, LLLinkGetData(lnk, StripNode, inlist));
}

static void
printSmesh(StripMesh *sm)
{
	for(int32 i = 0; i < sm->numNodes; i++)
		printNode(sm, &sm->nodes[i]);
}

static void
collectFaces(Geometry *geo, StripMesh *sm, uint16 m)
{
	StripNode *n;
	Triangle *t;
	sm->numNodes = 0;
	for(int32 i = 0; i < geo->numTriangles; i++){
		t = &geo->triangles[i];
		if(t->matId == m){
			n = &sm->nodes[sm->numNodes++];
			n->v[0] = t->v[0];
			n->v[1] = t->v[1];
			n->v[2] = t->v[2];
			n->e[0].node = 0;
			n->e[1].node = 0;
			n->e[2].node = 0;
			n->e[0].isConnected = 0;
			n->e[1].isConnected = 0;
			n->e[2].isConnected = 0;
			n->e[0].isStrip = 0;
			n->e[1].isStrip = 0;
			n->e[2].isStrip = 0;
			n->parent = 0;
			n->visited = 0;
			n->stripVisited = 0;
			n->isEnd = 0;
			n->stripId = -1;
			n->inlist.init();
		}
	}
}

/* Find Triangle that has edge e. */
static GraphEdge
findEdge(StripMesh *sm, int32 e[2])
{
	StripNode *n;
	GraphEdge ge = { 0, 0, 0, 0 };
	for(int32 i = 0; i < sm->numNodes; i++){
		n = &sm->nodes[i];
		for(int32 j = 0; j < 3; j++){
			if(n->e[j].isConnected)
				continue;
			if(e[0] == n->v[j] &&
			   e[1] == n->v[(j+1) % 3]){
				ge.node = i;
				ge.isConnected = 1;
				ge.otherEdge = j;
				return ge;
			}
		}
	}
	return ge;
}

/* Connect nodes sharing an edge, preserving winding */
static void
connectNodes(StripMesh *sm)
{
	StripNode *n, *nn;
	int32 e[2];
	GraphEdge ge;
	for(int32 i = 0; i < sm->numNodes; i++){
		n = &sm->nodes[i];
		for(int32 j = 0; j < 3; j++){
			if(n->e[j].isConnected)
				continue;

			/* flip edge and search for node */
			e[1] = n->v[j];
			e[0] = n->v[(j+1) % 3];
			ge = findEdge(sm, e);
			/* found node, now connect */
			if(ge.isConnected){
				n->e[j].node = ge.node;
				n->e[j].isConnected = 1;
				n->e[j].otherEdge = ge.otherEdge;
				n->e[j].isStrip = 0;
				nn = &sm->nodes[ge.node];
				nn->e[ge.otherEdge].node = i;
				nn->e[ge.otherEdge].isConnected = 1;
				nn->e[ge.otherEdge].otherEdge = j;
				nn->e[ge.otherEdge].isStrip = 0;
			}
		}
	}
}

static int32
numConnections(StripNode *n)
{
	return  n->e[0].isConnected +
		n->e[1].isConnected +
		n->e[2].isConnected;
}

static int32
numStripEdges(StripNode *n)
{
	return  n->e[0].isStrip +
		n->e[1].isStrip +
		n->e[2].isStrip;
}

#define IsEnd(n) (numConnections(n) > 0 && numStripEdges(n) < 2)

static void
complementEdge(StripMesh *sm, GraphEdge *e)
{
	e->isStrip = !e->isStrip;
	e = &sm->nodes[e->node].e[e->otherEdge];
	e->isStrip = !e->isStrip;
}

/* While possible extend a strip from a starting node until
 * we find a node already in a strip. N.B. this function does
 * make no attempts to connect to an already existing strip. */
static void
extendStrip(StripMesh *sm, StripNode *start)
{
	StripNode *n, *nn;
	n = start;
	if(numConnections(n) == 0){
		sm->loneNodes.append(&n->inlist);
		return;
	}
	sm->endNodes.append(&n->inlist);
	n->isEnd = 1;
tail:
	/* Find the next node to connect to on any of the three edges */
	for(int32 i = 0; i < 3; i++){
		if(!n->e[i].isConnected)
			continue;
		nn = &sm->nodes[n->e[i].node];
		if(nn->stripId >= 0)
			continue;

		/* found one */
		nn->stripId = n->stripId;
		complementEdge(sm, &n->e[i]);
		n = nn;
		goto tail;
	}
	if(n != start){
		sm->endNodes.append(&n->inlist);
		n->isEnd = 1;
	}
}

static void
buildStrips(StripMesh *sm)
{
	StripNode *n;
	for(int32 i = 0; i < sm->numNodes; i++){
		n = &sm->nodes[i];
		if(n->stripId >= 0)
			continue;
		n->stripId = i;
		extendStrip(sm, n);
	}
}

static StripNode*
findTunnel(StripMesh *sm, StripNode *n)
{
	LinkList searchNodes;
	StripNode *nn;
	int edgetype;
	int isEnd;

	searchNodes.init();
	edgetype = 0;
	for(;;){
		for(int32 i = 0; i < 3; i++){
			/* Find a node connected by the right edgetype */
			if(!n->e[i].isConnected ||
			    n->e[i].isStrip != edgetype)
				continue;
			nn = &sm->nodes[n->e[i].node];

			/* If the node has been visited already,
			 * there's a shorter path. */
			if(nn->visited)
				continue;

			/* Don't allow non-strip edge between nodes of the same
			 * strip to prevent loops.
			 * Actually these edges are allowed under certain
			 * circumstances, but they require complex checks. */
			if(edgetype == 0 &&
			   n->stripId == nn->stripId)
				continue;

			isEnd = IsEnd(nn);

			/* Can't add end nodes to two lists, so skip. */
			if(isEnd && edgetype == 1)
				continue;

			nn->parent = n->e[i].otherEdge;
			nn->visited = 1;
			sm->nodes[nn->stripId].stripVisited = 1;

			/* Search complete. */
			if(isEnd && edgetype == 0)
				return nn;

			/* Found a valid node. */
			searchNodes.append(&nn->inlist);
		}
		if(searchNodes.isEmpty())
			return nil;
		n = LLLinkGetData(searchNodes.link.next, StripNode, inlist);
		n->inlist.remove();
		edgetype = !edgetype;
	}
}

static void
resetGraph(StripMesh *sm)
{
	StripNode *n;
	for(int32 i = 0; i < sm->numNodes; i++){
		n = &sm->nodes[i];
		n->visited = 0;
		n->stripVisited = 0;
	}
}

static StripNode*
walkStrip(StripMesh *sm, StripNode *start)
{
	StripNode *n, *nn;
	int32 last;

//printf("stripend: ");
//printNode(sm, start);

	n = start;
	last = -1;
	for(;;n = nn){
		n->visited = 0;
		n->stripVisited = 0;
		if(n->isEnd)
			n->inlist.remove();
		n->isEnd = 0;

		if(IsEnd(n) && n != start)
			return n;

		/* find next node */
		nn = nil;
		for(int32 i = 0; i < 3; i++){
			if(!n->e[i].isStrip || i == last)
				continue;
			nn = &sm->nodes[n->e[i].node];
			last = n->e[i].otherEdge;
			nn->stripId = n->stripId;
			break;
		}
//printf("    next: ");
//printNode(sm, nn);
		if(nn == nil)
			return nil;
	}
}

static void
applyTunnel(StripMesh *sm, StripNode *end, StripNode *start)
{
	LinkList tmplist;
	StripNode *n, *nn;

	for(n = end; n != start; n = &sm->nodes[n->e[n->parent].node]){
//printf("	");
//printNode(sm, n);
		complementEdge(sm, &n->e[n->parent]);
	}
//printf("	");
//printNode(sm, start);

//printSmesh(sm);
//printf("-------\n");
	tmplist.init();
	while(!sm->endNodes.isEmpty()){
		n = LLLinkGetData(sm->endNodes.link.next, StripNode, inlist);
		/* take out of end list */
		n->inlist.remove();
		n->isEnd = 0;
		/* no longer an end node */
		if(!IsEnd(n))
			continue;
		// TODO: only walk strip if it was touched
		/* set new id, walk strip and find other end */
		n->stripId = n - sm->nodes;
		nn = walkStrip(sm, n);
		tmplist.append(&n->inlist);
		n->isEnd = 1;
		if(nn && n != nn){
			tmplist.append(&nn->inlist);
			nn->isEnd = 1;
		}
	}
	/* Move new end nodes to the real list. */
	sm->endNodes = tmplist;
	sm->endNodes.link.next->prev = &sm->endNodes.link;
	sm->endNodes.link.prev->next = &sm->endNodes.link;
}

static void
tunnel(StripMesh *sm)
{
	StripNode *n, *nn;

again:
	FORLIST(lnk, sm->endNodes){
		n = LLLinkGetData(lnk, StripNode, inlist);
//		printf("searching %p %d\n", n, numStripEdges(n));
		nn = findTunnel(sm, n);
//		printf("          %p %p\n", n, nn);

		if(nn){
			applyTunnel(sm, nn, n);
			resetGraph(sm);
			/* applyTunnel changes sm->endNodes, so we have to
			 * jump out of the loop. */
			goto again;
		}
		resetGraph(sm);
	}
	printf("tunneling done!\n");
}

/*
 * For each material:
 * 1. build dual graph (collectFaces, connectNodes)
 * 2. make some simple strip (buildStrips)
 * 3. apply tunnel operator (tunnel)
 */
void
Geometry::buildTristrips(void)
{
	StripMesh smesh;

	printf("%ld\n", sizeof(StripNode));

	smesh.nodes = new StripNode[this->numTriangles];
	for(int32 i = 0; i < this->matList.numMaterials; i++){
		smesh.loneNodes.init();
		smesh.endNodes.init();
		collectFaces(this, &smesh, i);
		connectNodes(&smesh);
		buildStrips(&smesh);
printSmesh(&smesh);
printf("-------\n");
//printLone(&smesh);
//printf("-------\n");
//printEnds(&smesh);
//printf("-------\n");
		tunnel(&smesh);
//printf("-------\n");
//printEnds(&smesh);
	}
	delete[] smesh.nodes;

	exit(1);
}

}
