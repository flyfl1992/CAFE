#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include <math.h>

extern "C" {
#include <utils_string.h>
#include <cafe_shell.h>
#include <tree.h>
#include <cafe.h>
#include <chooseln_cache.h>
};

#include <cafe_commands.h>
#include <lambda.h>
#include <reports.h>
#include <likelihood_ratio.h>
#include <pvalue.h>
#include <branch_cutting.h>
#include <conditional_distribution.h>
#include <simerror.h>
#include <error_model.h>
#include <Globals.h>
#include <viterbi.h>

extern "C" {
	extern pCafeParam cafe_param;
	void show_sizes(FILE*, pCafeTree pcafe, family_size_range*range, pCafeFamilyItem pitem, int i);
	void phylogeny_lambda_parse_func(pTree ptree, pTreeNode ptnode);
	extern pBirthDeathCacheArray probability_cache;
}


static void init_cafe_tree(Globals& globals)
{
	const char *newick_tree = "(((chimp:6,human:6):81,(mouse:17,rat:17):70):6,dog:9)";
	char buf[100];
	strcpy(buf, "tree ");
	strcat(buf, newick_tree);
	cafe_shell_dispatch_command(globals, buf);
}

static pCafeTree create_tree(family_size_range range)
{
	const char *newick_tree = "(((chimp:6,human:6):81,(mouse:17,rat:17):70):6,dog:9)";
	char tree[100];
	strcpy(tree, newick_tree);
	return cafe_tree_new(tree, &range, 0, 0);
}

TEST_GROUP(FirstTestGroup)
{
	family_size_range range;

	void setup()
	{
		srand(10);
		range.min = range.root_min = 0;
		range.max = range.root_max = 15;

	}

	void teardown()
	{
		MemoryLeakWarningPlugin::turnOnNewDeleteOverloads();
	}
};

TEST_GROUP(LikelihoodRatio)
{
	void setup()
	{
		srand(10);
	}
};

TEST_GROUP(TreeTests)
{
	family_size_range range;

	void setup()
	{
		srand(10);
		range.min = range.root_min = 0;
		range.max = range.root_max = 15;
	}
};

TEST_GROUP(ReportTests)
{
	void setup()
	{
		srand(10);
	}
};

TEST_GROUP(PValueTests)
{
	void setup()
	{
		srand(10);
	}

	void teardown()
	{
		// prevent the values in the static matrix from being reported as a memory leak
		std::vector<std::vector<double> >().swap(ConditionalDistribution::matrix);
	}
};

TEST(TreeTests, node_set_birthdeath_matrix)
{
	std::string str;

	pBirthDeathCacheArray cache = birthdeath_cache_init(10);
	pTree tree = (pTree)create_tree(range);
	pCafeNode node = (pCafeNode)tree_get_child(tree->root, 0);

	POINTERS_EQUAL(NULL, node->birthdeath_matrix);

	// if branch length is not set, no probabilities can be set
	node->super.branchlength = -1;
	node_set_birthdeath_matrix(node, cache, 0);
	POINTERS_EQUAL(NULL, node->birthdeath_matrix);

	// if param_lambdas not set, node's birthdeath matrix will be set
	node->super.branchlength = 6;
	node_set_birthdeath_matrix(node, cache, 0);
	CHECK(node->birthdeath_matrix != NULL)

		// if param_lambdas not set, node's birthdeath matrix will be set
	node->super.branchlength = 6;
	node_set_birthdeath_matrix(node, cache, 5);
	CHECK(node->birthdeath_matrix != NULL);

	// even if param_lambdas is set, node's birthdeath matrix will be set if num_lambdas is 0
	node->birthdeath_matrix = NULL;
	node->birth_death_probabilities.param_lambdas = (double*)memory_new(5, sizeof(double));
	node_set_birthdeath_matrix(node, cache, 0);
	CHECK(node->birthdeath_matrix != NULL);

	// if param_lambdas is set and num_lambdas > 0, put the matrices into k_bd
	node->birthdeath_matrix = NULL;
	node->k_bd = arraylist_new(5);
	node_set_birthdeath_matrix(node, cache, 5);
	POINTERS_EQUAL(NULL, node->birthdeath_matrix);
	LONGS_EQUAL(5, node->k_bd->size);
}

TEST(FirstTestGroup, TestStringSplitter)
{
	char c[10];
	c[0] = 0;
	LONGS_EQUAL(0, string_pchar_space_split(c)->size);

	pArrayList pArray;
	strcpy(c, "a b");
	pArray = string_pchar_space_split(c);
	LONGS_EQUAL(2, pArray->size);
	STRCMP_EQUAL("a", (char *)(pArray->array[0]));
	STRCMP_EQUAL("b", (char *)(pArray->array[1]));
}

TEST(FirstTestGroup, Tokenize)
{
	char c[10];
	c[0] = 0;
	LONGS_EQUAL(0, tokenize(c).size());
	strcpy(c, " ");
	LONGS_EQUAL(0, tokenize(c).size());

	strcpy(c, "a b\r\n");
	std::vector<std::string> arr = tokenize(c);
	LONGS_EQUAL(2, arr.size());
	STRCMP_EQUAL("a", arr[0].c_str());
	STRCMP_EQUAL("b", arr[1].c_str());
}

TEST(TreeTests, TestCafeTree)
{
	pCafeTree cafe_tree = create_tree(range);
	LONGS_EQUAL(104, cafe_tree->super.size); // tracks the size of the structure for copying purposes, etc.

	// Find chimp in the tree after two branches of length 6,81,6
	pTreeNode ptnode = (pTreeNode)cafe_tree->super.root;
	CHECK(tree_is_root(&cafe_tree->super, ptnode));
	ptnode = (pTreeNode)tree_get_child(ptnode, 0);
	pPhylogenyNode pnode = (pPhylogenyNode)ptnode;
	LONGS_EQUAL(6, pnode->branchlength);	// root to first branch = 6

	ptnode = (pTreeNode)tree_get_child(ptnode, 0);
	pnode = (pPhylogenyNode)ptnode;
	LONGS_EQUAL(81, pnode->branchlength); // 1st to 2nd = 81

	ptnode = (pTreeNode)tree_get_child(ptnode, 0);
	pnode = (pPhylogenyNode)ptnode;
	STRCMP_EQUAL("chimp", pnode->name);
	LONGS_EQUAL(6, pnode->branchlength);  // 2nd to chimp leaf = 6
	CHECK(tree_is_leaf(ptnode));


}

TEST(FirstTestGroup, TestShellDispatcher)
{
	char c[10];

	Globals globals;

	strcpy(c, "# a comment");
	LONGS_EQUAL(0, cafe_shell_dispatch_command(globals, c));

	strcpy(c, "unknown");
	LONGS_EQUAL(CAFE_SHELL_NO_COMMAND, cafe_shell_dispatch_command(globals, c));
}

TEST(FirstTestGroup, TestShowSizes)
{
	char outbuf[10000];
	setbuf(stdout, outbuf);

	CafeParam param;
	param.family_size.root_min = 29;
	param.family_size.root_max = 31;
	param.family_size.min = 37;
	param.family_size.max = 41;

	CafeFamilyItem item;
	item.ref = 14;
	CafeTree tree;
	tree.rootfamilysizes[0] = 11;
	tree.rootfamilysizes[1] = 13;
	tree.familysizes[0] = 23;
	tree.familysizes[1] = 19;
	tree.rfsize = 17;
	param.pcafe = &tree;
	FILE* in = fmemopen(outbuf, 999, "w");
	show_sizes(in, &tree, &param.family_size, &item, 7);
	fclose(in);
	STRCMP_CONTAINS(">> 7 14", outbuf);
	STRCMP_CONTAINS("Root size: 11 ~ 13 , 17", outbuf);
	STRCMP_CONTAINS("Family size: 23 ~ 19", outbuf);
	STRCMP_CONTAINS("Root size: 29 ~ 31", outbuf);
	STRCMP_CONTAINS("Family size: 37 ~ 41", outbuf);
}

TEST(FirstTestGroup, TestPhylogenyLoadFromString)
{
	char outbuf[10000];
	strcpy(outbuf, "(((1,1)1,(2,2)2)2,2)");
	Globals globals;
	init_cafe_tree(globals);
	pTree tree = phylogeny_load_from_string(outbuf, tree_new, phylogeny_new_empty_node, phylogeny_lambda_parse_func, 0);
	CHECK(tree != 0);
	LONGS_EQUAL(56, tree->size);

};

static pArrayList build_arraylist(const char *items[], int count)
{
	pArrayList psplit = arraylist_new(20);
	for (int i = 0; i < count; ++i)
	{
		char *str = (char*)memory_new(strlen(items[i]) + 1, sizeof(char));
		strcpy(str, items[i]);
		arraylist_add(psplit, str);
	}
	return psplit;
}

TEST(FirstTestGroup, Test_cafe_get_posterior)
{
	CafeParam param;
	param.flog = stdout;
	param.quiet = 1;
	param.prior_rfsize = NULL;
	param.pcafe = create_tree(range);
	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	param.pfamily = cafe_family_init(build_arraylist(species, 7));
	cafe_family_set_species_index(param.pfamily, param.pcafe);
	const char *values[] = { "description", "id", "3", "5", "7", "11", "13" };
	cafe_family_add_item(param.pfamily, build_arraylist(values, 7));

	param.ML = (double*)memory_new(15, sizeof(double));
	param.MAP = (double*)memory_new(15, sizeof(double));

	LONGS_EQUAL(16,	param.pcafe->size_of_factor);	// as a side effect of create_tree

	pArrayList node_list = param.pcafe->super.nlist;
	pTreeNode node = (pTreeNode)node_list->array[1];
	CHECK(node->children->head != NULL);

	family_size_range range;
	range.min = 0;
	range.max = 15;
	range.root_min = 0;
	range.root_max = 15;

	reset_birthdeath_cache(param.pcafe, 0, &range);

	DOUBLES_EQUAL(-1.0, cafe_get_posterior(param.pfamily, param.pcafe, &param.family_size, param.ML, param.MAP, param.prior_rfsize, param.quiet), 0.01);	// -1 represents an error - empirical posterior not defined. Is this safe?

	cafe_set_prior_rfsize_empirical(&param);
	CHECK_FALSE(isfinite(cafe_get_posterior(param.pfamily, param.pcafe, &param.family_size, param.ML, param.MAP, param.prior_rfsize, param.quiet)));
};

TEST(TreeTests, compute_internal_node_likelihoode)
{
	pCafeTree pcafe = create_tree(range);
	pCafeNode node = (pCafeNode)pcafe->super.nlist->array[3];
	probability_cache = birthdeath_cache_init(pcafe->size_of_factor);
	compute_internal_node_likelihood((pTree)pcafe, (pTreeNode)node);
	DOUBLES_EQUAL(0, node->likelihoods[0], .001);
}

TEST(FirstTestGroup, cafe_set_prior_rfsize_empirical)
{
	CafeParam param;
	param.flog = stdout;
	param.prior_rfsize = NULL;
	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	param.pfamily = cafe_family_init(build_arraylist(species, 7));
	const char *values[] = { "description", "id", "3", "5", "7", "11", "13" };
	cafe_family_add_item(param.pfamily, build_arraylist(values, 7));

	param.pcafe = create_tree(range);
	cafe_set_prior_rfsize_empirical(&param);
	DOUBLES_EQUAL(0.0, param.prior_rfsize[0], .001);
}

TEST(FirstTestGroup, list_commands)
{
	std::ostringstream ost;
	list_commands(ost);
	STRCMP_CONTAINS("lambda", ost.str().c_str());
	STRCMP_CONTAINS("tree", ost.str().c_str());
	STRCMP_CONTAINS("load", ost.str().c_str());
	STRCMP_CONTAINS("branchlength", ost.str().c_str());
}

void store_node_id(pTree ptree, pTreeNode ptnode, va_list ap1)
{
	va_list ap;
	va_copy(ap, ap1);
	std::vector<int> * ids = va_arg(ap, std::vector<int> *);
	ids->push_back(ptnode->id);
	va_end(ap);
}

TEST(FirstTestGroup, tree_traversal_prefix)
{
	std::vector<int> ids;
	pCafeTree tree = create_tree(range);
	tree_traveral_prefix(&tree->super, store_node_id, &ids);
	LONGS_EQUAL(7, ids[0]);
	LONGS_EQUAL(3, ids[1]);
	LONGS_EQUAL(1, ids[2]);
	LONGS_EQUAL(0, ids[3]);
	LONGS_EQUAL(2, ids[4]);
	LONGS_EQUAL(5, ids[5]);
	LONGS_EQUAL(4, ids[6]);
	LONGS_EQUAL(6, ids[7]);
	LONGS_EQUAL(8, ids[8]);
}

TEST(FirstTestGroup, tree_traversal_infix)
{
	std::vector<int> ids;
	pCafeTree tree = create_tree(range);
	tree_traveral_infix(&tree->super, store_node_id, &ids);
	LONGS_EQUAL(0, ids[0]);
	LONGS_EQUAL(1, ids[1]);
	LONGS_EQUAL(2, ids[2]);
	LONGS_EQUAL(3, ids[3]);
	LONGS_EQUAL(4, ids[4]);
	LONGS_EQUAL(5, ids[5]);
	LONGS_EQUAL(6, ids[6]);
	LONGS_EQUAL(7, ids[7]);
	LONGS_EQUAL(8, ids[8]);
}

TEST(FirstTestGroup, tree_traversal_postfix)
{
	std::vector<int> ids;
	pCafeTree tree = create_tree(range);
	tree_traveral_postfix(&tree->super, store_node_id, &ids);
	LONGS_EQUAL(0, ids[0]);
	LONGS_EQUAL(2, ids[1]);
	LONGS_EQUAL(1, ids[2]);
	LONGS_EQUAL(4, ids[3]);
	LONGS_EQUAL(6, ids[4]);
	LONGS_EQUAL(5, ids[5]);
	LONGS_EQUAL(3, ids[6]);
	LONGS_EQUAL(8, ids[7]);
	LONGS_EQUAL(7, ids[8]);
}

TEST(TreeTests, cafe_tree_random_probabilities)
{
	int num_families = 1;
	pCafeTree tree = create_tree(range);
	probability_cache = (pBirthDeathCacheArray)memory_new(num_families, sizeof(BirthDeathCacheArray));
	probability_cache->maxFamilysize = num_families;
	pArrayList node_list = tree->super.nlist;
	square_matrix bd;
	square_matrix_init(&bd, tree->familysizes[1] + 1);
	for (int i = 0; i < num_families+1; ++i)
	{
		square_matrix_set(&bd, i, 0, i);
	}
	for (int i = 0; i < node_list->size; ++i)
	{
		pCafeNode node = (pCafeNode)arraylist_get(node_list, i);
		node->birthdeath_matrix = &bd;
	}

	std::vector<double> trials = get_random_probabilities(tree, 1, 5);
	DOUBLES_EQUAL(0.0, trials[0], .001);
	DOUBLES_EQUAL(0.0, trials[1], .001);
	DOUBLES_EQUAL(0.0, trials[2], .001);
	DOUBLES_EQUAL(0.0, trials[3], .001);
	DOUBLES_EQUAL(0.0, trials[4], .001);
}

TEST(ReportTests, get_report_parameters)
{
	std::vector<std::string> tokens;
	tokens.push_back("report");
	tokens.push_back("myreport");
	tokens.push_back("branchcutting");

	report_parameters params = get_report_parameters(tokens);
	STRCMP_EQUAL("myreport", params.name.c_str());
	CHECK(params.branchcutting);
	CHECK_FALSE(params.likelihood);
	CHECK_FALSE(params.html);

	tokens[2] = "lh2";
	params = get_report_parameters(tokens);
	CHECK(params.lh2);
	CHECK_FALSE(params.likelihood);

	tokens.push_back("html");
	params = get_report_parameters(tokens);
	CHECK(params.html);
}

TEST(ReportTests, write_report)
{
	Globals globals;
	std::ostringstream ost;
	CafeFamily fam;
	fam.flist = arraylist_new(1);

	globals.viterbi->num_nodes = 0;
	family_size_range range;
	range.min = 0;
	range.max = 15;
	range.root_min = 0;
	range.root_max = 15;

	globals.param.pcafe = create_tree(range);
	globals.param.pfamily = &fam;
	probability_cache = NULL;
	globals.param.num_lambdas = 3;
	double lambdas[] = { 1.5, 2.5, 3.5 };
	globals.param.lambda = lambdas;
	globals.param.lambda_tree = (pTree)globals.param.pcafe;
	Report r(&globals.param, *globals.viterbi);
	ost << r;
	STRCMP_CONTAINS("Tree:(((chimp:6,human:6):81,(mouse:17,rat:17):70):6,dog:9)\n", ost.str().c_str());
	STRCMP_CONTAINS("Lambda:\t1.5\t2.5\t3.5\n", ost.str().c_str());
	STRCMP_CONTAINS("Lambda tree:\t(((:6,:6):81,(:17,:17):70):6,:9)\n", ost.str().c_str());
	STRCMP_CONTAINS("IDs of nodes:(((chimp<0>,human<2>)<1>,(mouse<4>,rat<6>)<5>)<3>,dog<8>)<7>\n", ost.str().c_str());
	STRCMP_CONTAINS("# Output format for: ' Average Expansion', 'Expansions', 'No Change', 'Contractions', and 'Branch-specific P-values' = (node ID, node ID): ", ost.str().c_str());
	STRCMP_CONTAINS("(0,2) (1,5) (4,6) (3,8) \n", ost.str().c_str());
	STRCMP_CONTAINS("# Output format for 'Branch cutting P-values' and 'Likelihood Ratio Test': (0, 1, 2, 3, 4, 5, 6, 7, 8)\n", ost.str().c_str());
	STRCMP_CONTAINS("", ost.str().c_str());
}

TEST(ReportTests, write_viterbi)
{
	std::ostringstream ost;
	Report r;
	for (int i = 1; i < 7; ++i)
		r.averageExpansion.push_back(i + 0.3);

	r.changes.push_back(change(1,17,41));
	r.changes.push_back(change(3, 19, 43));
	r.changes.push_back(change(5, 23, 47));
	r.changes.push_back(change(7, 29, 53));
	r.changes.push_back(change(11, 31, 59));
	r.changes.push_back(change(13, 37, 61));

	write_viterbi(ost, r);

	STRCMP_CONTAINS("Average Expansion:\t(1.3,2.3)\t(3.3,4.3)\t(5.3,6.3)\n", ost.str().c_str());
	STRCMP_CONTAINS("Expansion :\t(1,3)\t(5,7)\t(11,13)\n", ost.str().c_str());
	STRCMP_CONTAINS("Remain :\t(17,19)\t(23,29)\t(31,37)\n", ost.str().c_str());
	STRCMP_CONTAINS("Decrease :\t(41,43)\t(47,53)\t(59,61)\n", ost.str().c_str());
}


TEST(ReportTests, write_families_header)
{
	std::ostringstream ost;
	write_families_header(ost, NULL, NULL);
	STRCMP_EQUAL("'ID'\t'Newick'\t'Family-wide P-value'\t'Viterbi P-values'\n", ost.str().c_str());

	std::ostringstream ost2;
	write_families_header(ost2, (double **)1, NULL);
	STRCMP_EQUAL("'ID'\t'Newick'\t'Family-wide P-value'\t'Viterbi P-values'\t'cut P-value'\n", ost2.str().c_str());

	std::ostringstream ost3;
	write_families_header(ost3, (double **)1, (double **)1);
	STRCMP_EQUAL("'ID'\t'Newick'\t'Family-wide P-value'\t'Viterbi P-values'\t'cut P-value'\t'Likelihood Ratio'\n", ost3.str().c_str());
}

TEST(ReportTests, write_families_line)
{
	Globals globals;
	globals.param.likelihoodRatios = NULL;
	family_size_range range;
	range.min = range.root_min = 0;
	range.max = range.root_max = 10;
	pCafeTree tree = create_tree(range);
	globals.param.pcafe = tree;
	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	globals.param.pfamily = cafe_family_init(build_arraylist(species, 7));
	cafe_family_set_species_index(globals.param.pfamily, tree);
	const char *values[] = { "description", "id", "3", "5", "7", "11", "13" };
	cafe_family_add_item(globals.param.pfamily, build_arraylist(values, 7));

	viterbi_parameters* v = globals.viterbi;
	v->num_nodes = 6;
	//double expansion[] = { 1.3, 2.3, 3.3, 4.3, 5.3, 6.3 };
	v->viterbiPvalues = (double**)memory_new_2dim(6, 1, sizeof(double));
	v->viterbiPvalues[0][0] = .025;
	v->viterbiNodeFamilysizes = (int**)memory_new_2dim(6, 1, sizeof(int));
	v->cutPvalues = NULL;

	double maxP = .1;
	v->maximumPvalues = &maxP;

	std::ostringstream ost;
	family_line_item item(globals.param.pfamily, globals.param.pcafe, globals.param.likelihoodRatios, *globals.viterbi, 0, "NodeZero");
	ost << item;
	STRCMP_EQUAL("NodeZero\t(((chimp_3:6,human_5:6)_0:81,(mouse_7:17,rat_11:17)_0:70)_0:6,dog_13:9)_0\t0.1\t((0.025,0),(0,0),(0,0))\t", ost.str().c_str());

	v->cutPvalues = (double**)memory_new_2dim(6, 1, sizeof(double));
	std::ostringstream ost2;
	family_line_item item2(globals.param.pfamily, globals.param.pcafe, globals.param.likelihoodRatios, *globals.viterbi, 0, "NodeZero");
	ost2 << item2;
	STRCMP_EQUAL("NodeZero\t(((chimp_3:6,human_5:6)_0:81,(mouse_7:17,rat_11:17)_0:70)_0:6,dog_13:9)_0\t0.1\t((0.025,0),(0,0),(0,0))\t(0,0,0,0,0,0)\t", ost2.str().c_str());

	v->cutPvalues = NULL;
	globals.param.likelihoodRatios = (double**)memory_new_2dim(tree->super.size, 1, sizeof(double));
	std::ostringstream ost3;
	family_line_item item3(globals.param.pfamily, globals.param.pcafe, globals.param.likelihoodRatios, *globals.viterbi, 0, "NodeZero");
	ost3 << item3;
	STRCMP_EQUAL("NodeZero\t(((chimp_3:6,human_5:6)_0:81,(mouse_7:17,rat_11:17)_0:70)_0:6,dog_13:9)_0\t0.1\t((0.025,0),(0,0),(0,0))\t(0,0,0,0,0,0,0,0,0)", ost3.str().c_str());
}

TEST(FirstTestGroup, cafe_tree_new_empty_node)
{
	pTree tree = (pTree)create_tree(range);
	pCafeNode node = (pCafeNode)cafe_tree_new_empty_node(tree);
	POINTERS_EQUAL(NULL, node->errormodel);
	POINTERS_EQUAL(NULL, node->birthdeath_matrix);
	POINTERS_EQUAL(NULL, node->k_bd);
	POINTERS_EQUAL(NULL, node->k_likelihoods);
	POINTERS_EQUAL(NULL, node->birth_death_probabilities.param_mus);
	POINTERS_EQUAL(NULL, node->birth_death_probabilities.param_lambdas);
	LONGS_EQUAL(-1, node->familysize);
}

TEST(FirstTestGroup, chooseln_cache)
{
	struct chooseln_cache cache;
	cache.values = 0;
	CHECK_FALSE(chooseln_is_init2(&cache));
	chooseln_cache_init2(&cache, 10);
	CHECK_TRUE(chooseln_is_init2(&cache));
	LONGS_EQUAL(10, get_chooseln_cache_size2(&cache));
	DOUBLES_EQUAL(4.025, chooseln_get2(&cache, 8, 5), .001);
	DOUBLES_EQUAL(1.098, chooseln_get2(&cache, 3, 2), .001);
	DOUBLES_EQUAL(1.791, chooseln_get2(&cache, 6, 5), .001);
	DOUBLES_EQUAL(4.43, chooseln_get2(&cache, 9, 3), .001);
	chooseln_cache_free2(&cache);
}

TEST(FirstTestGroup, birthdeath_rate_with_log_alpha	)
{
	struct chooseln_cache cache;
	chooseln_cache_init2(&cache, 50);
	DOUBLES_EQUAL(0.107, birthdeath_rate_with_log_alpha(40, 42, -1.37, 0.5, &cache), .001);
	DOUBLES_EQUAL(0.006, birthdeath_rate_with_log_alpha(41, 34, -1.262, 0.4, &cache), .001);


}

TEST(FirstTestGroup, birthdeath_likelihood_with_s_c)
{
	struct chooseln_cache cache;
	chooseln_cache_init2(&cache, 50);
	DOUBLES_EQUAL(0.083, birthdeath_likelihood_with_s_c(40, 42, 0.42, 0.5, -1, &cache), .001);
	DOUBLES_EQUAL(0.023, birthdeath_likelihood_with_s_c(41, 34, 0.54, 0.4, -1, &cache), .001);
}

TEST(FirstTestGroup, square_matrix_resize)
{
	square_matrix matrix;
	square_matrix_init(&matrix, 2);
	square_matrix_set(&matrix, 0, 0, 1);
	square_matrix_set(&matrix, 0, 1, 2);
	square_matrix_set(&matrix, 1, 0, 3);
	square_matrix_set(&matrix, 1, 1, 4);
	square_matrix_resize(&matrix, 3);
	LONGS_EQUAL(1, square_matrix_get(&matrix, 0, 0));
	LONGS_EQUAL(2, square_matrix_get(&matrix, 0, 1));
	LONGS_EQUAL(3, square_matrix_get(&matrix, 1, 0));
	LONGS_EQUAL(4, square_matrix_get(&matrix, 1, 1));
	square_matrix_resize(&matrix, 1);
	LONGS_EQUAL(1, square_matrix_get(&matrix, 0, 0));
}
TEST(FirstTestGroup, compute_birthdeath_rates)
{
	chooseln_cache_init(3);
	struct square_matrix* matrix = compute_birthdeath_rates(10, 0.02, 0.01, 3);
	LONGS_EQUAL(4, matrix->size);

	// matrix.values should be set to a 3x3 array of doubles
	DOUBLES_EQUAL(1, square_matrix_get(matrix, 0, 0), 0.001);
	DOUBLES_EQUAL(0, square_matrix_get(matrix, 0, 1), 0.001);
	DOUBLES_EQUAL(0, square_matrix_get(matrix, 0, 2), 0.001);
	DOUBLES_EQUAL(.086, square_matrix_get(matrix, 1, 0), 0.001);
	DOUBLES_EQUAL(.754, square_matrix_get(matrix, 1, 1), 0.001);
	DOUBLES_EQUAL(.131, square_matrix_get(matrix, 1, 2), 0.001);
	DOUBLES_EQUAL(.007, square_matrix_get(matrix, 2, 0), 0.001);
	DOUBLES_EQUAL(.131, square_matrix_get(matrix, 2, 1), 0.001);
	DOUBLES_EQUAL(.591, square_matrix_get(matrix, 2, 2), 0.001);
}

TEST(FirstTestGroup, clear_tree_viterbis)
{
	pCafeTree tree = create_tree(range);
	pCafeNode pcnode = (pCafeNode)tree->super.nlist->array[4];
	pcnode->familysize = 5;
	pcnode->viterbi[0] = 9;
	pcnode->viterbi[1] = 13;
	clear_tree_viterbis(tree);
	DOUBLES_EQUAL(0.0, pcnode->viterbi[0], 0.0001);
	DOUBLES_EQUAL(0.0, pcnode->viterbi[1], 0.0001);
	LONGS_EQUAL(0, pcnode->familysize);
}

int get_family_size(pTree ptree, int id)
{
	for (int i = 0; i < ptree->nlist->size; ++i)
	{
		pCafeNode node = (pCafeNode)ptree->nlist->array[i];
		if (node->super.super.id == id)
			return node->familysize;
	}
	return -1;
}
TEST(FirstTestGroup, cafe_tree_random_familysize)
{
	pCafeTree tree = create_tree(range);
	probability_cache = NULL;
	CafeParam param;
	param.pcafe = tree;

	family_size_range range;
	range.min = 1;
	range.max = 10;
	range.root_min = 1;
	range.root_max = 3;
	reset_birthdeath_cache(param.pcafe, 0, &range);
	pCafeNode node5 = (pCafeNode)tree->super.nlist->array[5];
	for (int i = 0; i <= 10; ++i)
		for (int j = 0; j <= 10; ++j)
			square_matrix_set(node5->birthdeath_matrix, i, j, .1);

	int max = cafe_tree_random_familysize(tree, 10);
	LONGS_EQUAL(10, max);
	LONGS_EQUAL(10, get_family_size((pTree)tree, 3));
	LONGS_EQUAL(10, get_family_size((pTree)tree, 7));
	LONGS_EQUAL(8, get_family_size((pTree)tree, 5));
}

TEST(FirstTestGroup, reset_birthdeath_cache)
{
	pCafeTree tree = create_tree(range);
	probability_cache = NULL;
	family_size_range range;
	range.min = 0;
	range.root_min  = 0;
	range.max = 10;
	range.root_max = 10;
	
	reset_birthdeath_cache(tree, 0, &range);
	CHECK_FALSE(probability_cache == NULL);
	// every node should have its birthdeath_matrix set to an entry in the cache
	// matching its branch length, lambda and mu values
	pCafeNode node = (pCafeNode)tree->super.nlist->array[3];
	struct square_matrix* expected = birthdeath_cache_get_matrix(probability_cache, node->super.branchlength, node->birth_death_probabilities.lambda, node->birth_death_probabilities.mu);
	struct square_matrix* actual = node->birthdeath_matrix;
	LONGS_EQUAL(expected->size, actual->size);
	for (int i = 0; i < expected->size; ++i)
		for (int j = 0; j < expected->size; ++j)
			DOUBLES_EQUAL(square_matrix_get(expected, i, j), square_matrix_get(actual, i, j), .0001);
}

TEST(FirstTestGroup, get_num_trials)
{
	std::vector<std::string> tokens;
	LONGS_EQUAL(1, get_num_trials(tokens));
	tokens.push_back("not much");
	LONGS_EQUAL(1, get_num_trials(tokens));
	tokens.push_back("-t");
	tokens.push_back("17");
	LONGS_EQUAL(17, get_num_trials(tokens));
}

TEST(FirstTestGroup, initialize_leaf_likelihoods)
{
	int rows = 5;
	int cols = 3;
	double **matrix = (double**)memory_new_2dim(rows, cols, sizeof(double));
	initialize_leaf_likelihoods_for_viterbi(matrix, rows, 3, 1, cols, NULL);
	double expected[5][3] = { { 0, 1, 0},{0,1,0},{0,1,0},{0,1,0},{0,1,0}};
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < cols; ++j)
			DOUBLES_EQUAL(expected[i][j], matrix[i][j], .001);


	for (int i = 0; i < rows; ++i)
		expected[i][0] = 1;
	initialize_leaf_likelihoods_for_viterbi(matrix, rows, 2, -1, cols, NULL);
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < cols; ++j)
			DOUBLES_EQUAL(expected[i][j], matrix[i][j], .001);
}

TEST(FirstTestGroup, get_clusters)
{
	double k_weights[3] = { 1,2,3 };
	get_clusters(3, 1, k_weights);
}

TEST(FirstTestGroup, write_node_headers)
{
	pCafeTree tree = create_tree(range);
	std::ostringstream ost1, ost2;
	write_node_headers(ost1, ost2, tree);
	STRCMP_EQUAL("DESC\tFID\tchimp\thuman\tmouse\trat\tdog\n", ost1.str().c_str());
	STRCMP_EQUAL("DESC\tFID\tchimp\t-1\thuman\t-3\tmouse\t-5\trat\t-7\tdog\n", ost2.str().c_str());
}

TEST(FirstTestGroup, write_version)
{
	std::ostringstream ost;
	write_version(ost);
	STRCMP_CONTAINS("Version: 3.2, built at", ost.str().c_str());
}

TEST(FirstTestGroup, compute_viterbis)
{
	square_matrix matrix;
	square_matrix_init(&matrix, 6);
	square_matrix_set(&matrix, 0, 0, 1);
	square_matrix_set(&matrix, 0, 1, 2);
	square_matrix_set(&matrix, 1, 0, 3);
	square_matrix_set(&matrix, 1, 1, 4);
	CafeNode node;
	node.k_bd = arraylist_new(5);
	arraylist_add(node.k_bd, &matrix);
	arraylist_add(node.k_bd, &matrix);
	node.k_likelihoods = NULL;
	reset_k_likelihoods(&node, 5, 5);
	node.k_likelihoods[0][0] = 5;
	node.k_likelihoods[0][1] = 6;
	node.k_likelihoods[0][2] = 7;
	node.k_likelihoods[0][3] = 8;
	double factors[5];
	int viterbis[10];
	node.viterbi = viterbis;
	compute_viterbis(&node, 0, factors, 0, 1, 0, 1);

	LONGS_EQUAL(1, node.viterbi[0]);
	LONGS_EQUAL(1, node.viterbi[1]);

	DOUBLES_EQUAL(12, factors[0], .001);
	DOUBLES_EQUAL(24, factors[1], .001);
}
void set_familysize_to_node_times_three(pTree ptree, pTreeNode pnode, va_list ap1)
{
	((pCafeNode)pnode)->familysize = pnode->id * 3;
}

TEST(FirstTestGroup, write_leaves)
{
	pCafeTree tree = create_tree(range);
	tree_traveral_infix((pTree)tree, set_familysize_to_node_times_three, NULL);
	std::ostringstream ost1, ost2;
	int id = 1234;
	int i = 42;
	write_leaves(ost1, tree, NULL, i, id, true);
	STRCMP_EQUAL("root42\t1234\t0\t6\t12\t18\t24\n", ost1.str().c_str());
	write_leaves(ost2, tree, NULL, i, id, false);
	STRCMP_EQUAL("root42\t1234\t0\t3\t6\t9\t12\t15\t18\t21\t24\n", ost2.str().c_str());

	std::ostringstream ost3, ost4;
	int k = 5;
	write_leaves(ost3, tree, &k, i, id, true);
	STRCMP_EQUAL("k5_root42\t1234\t0\t6\t12\t18\t24\n", ost3.str().c_str());

	write_leaves(ost4, tree, &k, i, id, false);
	STRCMP_EQUAL("k5_root42\t1234\t0\t3\t6\t9\t12\t15\t18\t21\t24\n", ost4.str().c_str());
}

TEST(FirstTestGroup, run_viterbi_sim)
{
	pCafeTree tree = create_tree(range);
	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	pCafeFamily pfamily = cafe_family_init(build_arraylist(species, 7));
	cafe_family_set_species_index(pfamily, tree);
	const char *values[] = { "description", "id", "3", "5", "7", "11", "13" };
	cafe_family_add_item(pfamily, build_arraylist(values, 7));
	probability_cache = birthdeath_cache_init(tree->size_of_factor);
	cafe_tree_set_birthdeath(tree);

	roots roots;
	run_viterbi_sim(tree, pfamily, roots);
	LONGS_EQUAL(0, roots.size[0]);
	DOUBLES_EQUAL(0, roots.extinct[0], .01);
	LONGS_EQUAL(0, roots.total_extinct);
	LONGS_EQUAL(1, roots.num[0]);

}

TEST(FirstTestGroup, init_histograms)
{
	roots roots;
	roots.num.resize(2);
	roots.num[1] = 1;
	roots.extinct.resize(2);
	int maxsize = init_histograms(1, roots, 1);
	LONGS_EQUAL(1, maxsize);
	CHECK(roots.phist_sim[0] != NULL);
	CHECK(roots.phist_data[0] != NULL);
	CHECK(roots.phist_sim[1] != NULL);
	CHECK(roots.phist_data[1] != NULL);
}

TEST(FirstTestGroup, init_family_size)
{
	family_size_range sz;
	init_family_size(&sz, 100);
	LONGS_EQUAL(1, sz.root_min);
	LONGS_EQUAL(125, sz.root_max);
	LONGS_EQUAL(0, sz.min);
	LONGS_EQUAL(150, sz.max);

	init_family_size(&sz, 10);
	LONGS_EQUAL(1, sz.root_min);
	LONGS_EQUAL(30, sz.root_max);
	LONGS_EQUAL(0, sz.min);
	LONGS_EQUAL(60, sz.max);
}

TEST(FirstTestGroup, cafe_tree_set_parameters)
{
	pCafeTree tree = create_tree(range);

	family_size_range range;
	range.min = 0;
	range.max = 50;
	range.root_min = 15;
	range.root_max = 20;
	cafe_tree_set_parameters(tree, &range, 0.05);

	DOUBLES_EQUAL(0.05, tree->lambda, 0.0001);
	LONGS_EQUAL(0, tree->familysizes[0]);
	LONGS_EQUAL(50, tree->familysizes[1]);
	LONGS_EQUAL(15, tree->rootfamilysizes[0]);
	LONGS_EQUAL(20, tree->rootfamilysizes[1]);

	LONGS_EQUAL(51, tree->size_of_factor);
	// TODO: test that each node's likelihood and viterbi values have been reset to a size of 51
}

TEST(FirstTestGroup, cut_branch)
{
	MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
	probability_cache = NULL;

	range.min = 1;
	range.max = 15;
	range.root_min = 1;
	range.root_max = 15;
	pCafeTree tree = create_tree(range);
	int nnodes = ((pTree)tree)->nlist->size;
	CutBranch cb(nnodes);

	std::ostringstream ost;

	reset_birthdeath_cache(tree, 0, &range);

	range.max = 5;
	range.root_max = 5;

	const int node_id = 3;
	cut_branch(cb, (pTree)tree, tree, range, 1, 5, node_id, ost);

	STRCMP_CONTAINS(">> 3  --------------------\n", ost.str().c_str());
	STRCMP_CONTAINS("((chimp:6,human:6):81,(mouse:17,rat:17):70)\n", ost.str().c_str());
	STRCMP_CONTAINS("dog\n", ost.str().c_str());

	matrix& cd = cb.pCDSs[node_id].first;
	DOUBLES_EQUAL(0, cd[0][0], 0.001);

	CHECK(cb.pCDSs[node_id].second.empty());
	//POINTERS_EQUAL(NULL, cb.pCDSs[node_id].second);

}

TEST(FirstTestGroup, conditional_distribution)
{
	pCafeTree tree = create_tree(range);
	reset_birthdeath_cache(tree, 0, &range);

	std::vector<std::vector<double> > cd = conditional_distribution(tree, 0, 1, 1);

	// size of vector is range_end - range_start + 1
	LONGS_EQUAL(2, cd.size());
}

TEST(FirstTestGroup, set_size_for_split)
{
	pCafeTree tree = create_tree(range);

	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	pCafeFamily pfamily = cafe_family_init(build_arraylist(species, 7));
	cafe_family_set_species_index(pfamily, tree);
	const char *values[] = { "description", "id", "3", "5", "7", "11", "13" };
	cafe_family_add_item(pfamily, build_arraylist(values, 7));

	set_size_for_split(pfamily, 0, tree);

	LONGS_EQUAL(3, ((pCafeNode)(pPhylogenyNode)tree->super.nlist->array[0])->familysize);
	LONGS_EQUAL(5, ((pCafeNode)(pPhylogenyNode)tree->super.nlist->array[2])->familysize);
	LONGS_EQUAL(7, ((pCafeNode)(pPhylogenyNode)tree->super.nlist->array[4])->familysize);
	LONGS_EQUAL(11, ((pCafeNode)(pPhylogenyNode)tree->super.nlist->array[6])->familysize);
	LONGS_EQUAL(13, ((pCafeNode)(pPhylogenyNode)tree->super.nlist->array[8])->familysize);
}

TEST(FirstTestGroup, compute_cutpvalues)
{
	pCafeTree tree = create_tree(range);

	probability_cache = NULL;
	reset_birthdeath_cache(tree, 0, &range);

	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	pCafeFamily pfamily = cafe_family_init(build_arraylist(species, 7));
	cafe_family_set_species_index(pfamily, tree);
	const char *values[] = { "description", "id", "3", "5", "7", "11", "13" };
	cafe_family_add_item(pfamily, build_arraylist(values, 7));

	int nnodes = ((pTree)tree)->nlist->size;
	viterbi_parameters viterbi;
	viterbi_parameters_init(&viterbi, nnodes, 1);
	LONGS_EQUAL(nnodes, viterbi.averageExpansion.size());
	LONGS_EQUAL(nnodes, viterbi.expandRemainDecrease.size());
	double* p1 = (double*)memory_new(5, sizeof(double));
	double** p2 = (double**)memory_new_2dim(5, 5, sizeof(double));

	viterbi.cutPvalues = (double**)memory_new_2dim(6, 1, sizeof(double));

	CutBranch cb(nnodes);
	for (int i = 0; i < nnodes; ++i)
	{
		for (int j = 0; j < tree->rfsize; ++j)
		{
			cb.pCDSs[i].first.push_back(std::vector<double>(1, .5));
			cb.pCDSs[i].second.push_back(std::vector<double>(1, .5));
		}
	}
	compute_cutpvalues(tree, pfamily, 5, 0, 0, 1, viterbi, 0.05, p1, p2, cb);
	DOUBLES_EQUAL(0.6, viterbi.cutPvalues[0][0], .001);
}

TEST(FirstTestGroup, simulate_misclassification)
{
	const char *species[] = { "", "", "chimp" };
	const int num_species = 3;

	pCafeFamily pfamily = cafe_family_init(build_arraylist(species, num_species));
	const char *values[] = { "description", "id", "3" };
	cafe_family_add_item(pfamily, build_arraylist(values, num_species));

	ErrorStruct e;
	e.maxfamilysize = 4;
	e.errormatrix = (double**)memory_new_2dim(5, 5, sizeof(double));
	for (int i = 0; i < 5; i++) {
		e.errormatrix[i][3] = .4;	// 3 is gene count in chimp
	}
	std::vector<pErrorStruct> v(1);
	v[0] = &e;
	pfamily->error_ptr = &v[0];

	simulate_misclassification(pfamily);

	std::ostringstream ost;
	write_species_counts(pfamily, ost);

	STRCMP_CONTAINS("Desc\tFamily ID\tchimp\n", ost.str().c_str());
	STRCMP_CONTAINS("description\tid\t1\n", ost.str().c_str());

}

TEST(FirstTestGroup, get_random)
{
	std::vector<double> v(5, 0.2);

	LONGS_EQUAL(2, get_random(v));
}

TEST(FirstTestGroup, tree_set_branch_lengths)
{
	pCafeTree tree = create_tree(range);

	std::vector<int> lengths;
	try
	{
		tree_set_branch_lengths(tree, lengths);
		FAIL("No exception was thrown");
	}
	catch (const std::runtime_error& err)
	{
		STRCMP_EQUAL("ERROR: There are 9 branches including the empty branch of root\n", err.what());
	}
	int v[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	lengths.resize(9);
	std::copy(v, v + 9, lengths.begin());
	tree_set_branch_lengths(tree, lengths);
	pPhylogenyNode pnode = (pPhylogenyNode)tree->super.nlist->array[5];
	LONGS_EQUAL(pnode->branchlength, 5);
}

TEST(FirstTestGroup, initialize_k_bd_no_lambda)
{
	pCafeTree tree = create_tree(range);
	std::vector<double> values(10);
	values[0] = 0.05;

	CafeParam param;
	param.pcafe = tree;
	param.lambda_tree = NULL;
	param.parameterized_k_value = 0;
	initialize_k_bd(&param, &values[0]);

	// The following assertions should be true for every node in the tree
	pCafeNode node = (pCafeNode)tree->super.nlist->array[0];
	DOUBLES_EQUAL(.05, node->birth_death_probabilities.lambda, .0001);
	DOUBLES_EQUAL(-1, node->birth_death_probabilities.mu, .0001);
	CHECK(node->k_likelihoods == NULL);
	CHECK(node->k_bd == NULL);

	param.parameterized_k_value = 2;
	initialize_k_bd(&param, &values[0]);
	// The following assertions should be true for every node in the tree
	DOUBLES_EQUAL(-1, node->birth_death_probabilities.lambda, .0001);
	DOUBLES_EQUAL(-1, node->birth_death_probabilities.mu, .0001);
	CHECK_FALSE(node->k_likelihoods == NULL);
	CHECK_FALSE(node->k_bd == NULL);
}

TEST(FirstTestGroup, initialize_k_bd_with_lambda)
{
	pCafeTree tree = create_tree(range);
	pCafeTree lambda = create_tree(range);

	CafeParam param;
	param.pcafe = tree;
	param.lambda_tree = (pTree)lambda;
	param.parameterized_k_value = 0;
	param.fixcluster0 = 0;
	std::vector<double> values(10);
	values[0] = 0.05;

	for (int i = 0; i < lambda->super.nlist->size; ++i)
	{
		pPhylogenyNode node = (pPhylogenyNode)lambda->super.nlist->array[i];
		node->taxaid = 0;
	}

	initialize_k_bd(&param, &values[0]);

	// The following assertions should be true for every node in the tree
	pCafeNode node = (pCafeNode)tree->super.nlist->array[0];
	DOUBLES_EQUAL(.05, node->birth_death_probabilities.lambda, .0001);
	DOUBLES_EQUAL(-1, node->birth_death_probabilities.mu, .0001);
	CHECK(node->k_likelihoods == NULL);
	CHECK(node->k_bd == NULL);

	param.parameterized_k_value = 2;
	initialize_k_bd(&param, &values[0]);

	// The following assertions should be true for every node in the tree
	DOUBLES_EQUAL(-1, node->birth_death_probabilities.lambda, .0001);
	DOUBLES_EQUAL(-1, node->birth_death_probabilities.mu, .0001);
	CHECK_FALSE(node->k_likelihoods == NULL);
	CHECK_FALSE(node->k_bd == NULL);
}

TEST(FirstTestGroup, cafe_shell_clear_param_clears_probability_cache)
{
	Globals globals;
	memset(&globals.param, 0, sizeof(CafeParam));
	globals.param.pcafe = create_tree(range);
	globals.param.old_branchlength = (int *)calloc(1, sizeof(int)); // ?
	probability_cache = NULL;
	reset_birthdeath_cache(globals.param.pcafe, 1, &range);
	CHECK(probability_cache != NULL);

	globals.Clear(0);

	POINTERS_EQUAL(NULL, probability_cache);
}

TEST(FirstTestGroup, set_birth_death_probabilities3)
{
	struct probabilities probs;
	probs.lambda = 0;
	probs.mu = 0;
	probs.param_lambdas = NULL;
	probs.param_mus = NULL;
	std::vector<double> values(10);
	values[0] = .05;
	values[1] = .04;
	values[2] = .03;
	values[3] = .02;
	values[4] = .01;
	values[5] = .15;
	values[6] = .14;
	values[7] = .13;
	values[8] = .12;
	values[9] = .11;

	set_birth_death_probabilities4(&probs, -1, 0, 0, &values[0]);
	DOUBLES_EQUAL(.05, probs.lambda, .0001);
	DOUBLES_EQUAL(-1, probs.mu, .0001);

	set_birth_death_probabilities4(&probs, 5, 0, 0, &values[0]);
	DOUBLES_EQUAL(-1, probs.lambda, .0001);
	DOUBLES_EQUAL(-1, probs.mu, .0001);
	POINTERS_EQUAL(NULL, probs.param_mus);
	DOUBLES_EQUAL(.05, probs.param_lambdas[0], .0001);
	DOUBLES_EQUAL(.04, probs.param_lambdas[1], .0001);
	DOUBLES_EQUAL(.03, probs.param_lambdas[2], .0001);
	DOUBLES_EQUAL(.02, probs.param_lambdas[3], .0001);
	DOUBLES_EQUAL(.01, probs.param_lambdas[4], .0001);

	set_birth_death_probabilities4(&probs, 5, -1, 0, &values[0]);
	DOUBLES_EQUAL(-1, probs.lambda, .0001);
	DOUBLES_EQUAL(-1, probs.mu, .0001);
	POINTERS_EQUAL(NULL, probs.param_mus);
	DOUBLES_EQUAL(0, probs.param_lambdas[0], .0001);
	DOUBLES_EQUAL(.05, probs.param_lambdas[1], .0001);
	DOUBLES_EQUAL(.04, probs.param_lambdas[2], .0001);
	DOUBLES_EQUAL(.03, probs.param_lambdas[3], .0001);
	DOUBLES_EQUAL(.02, probs.param_lambdas[4], .0001);

	set_birth_death_probabilities4(&probs, 5, 0, 1, &values[0]);
	DOUBLES_EQUAL(-1, probs.lambda, .0001);
	DOUBLES_EQUAL(-1, probs.mu, .0001);
	POINTERS_EQUAL(NULL, probs.param_mus);
	DOUBLES_EQUAL(.15, probs.param_lambdas[0], .0001);
	DOUBLES_EQUAL(.14, probs.param_lambdas[1], .0001);
	DOUBLES_EQUAL(.13, probs.param_lambdas[2], .0001);
	DOUBLES_EQUAL(.12, probs.param_lambdas[3], .0001);
	DOUBLES_EQUAL(.11, probs.param_lambdas[4], .0001);

	set_birth_death_probabilities4(&probs, 5, -1, 1, &values[0]);
	DOUBLES_EQUAL(-1, probs.lambda, .0001);
	DOUBLES_EQUAL(-1, probs.mu, .0001);
	POINTERS_EQUAL(NULL, probs.param_mus);
	DOUBLES_EQUAL(.0, probs.param_lambdas[0], .0001);
	DOUBLES_EQUAL(.01, probs.param_lambdas[1], .0001);
	DOUBLES_EQUAL(.15, probs.param_lambdas[2], .0001);
	DOUBLES_EQUAL(.14, probs.param_lambdas[3], .0001);
	DOUBLES_EQUAL(.13, probs.param_lambdas[4], .0001);
}

TEST(FirstTestGroup, initialize_k_weights)
{
	CafeParam param;
	std::vector<double> weights(10);
	std::vector<double> params(100);
	for (double i = 0; i < 100; ++i)
		params[i] = i / 100.0;
	param.k_weights = &weights[0];
	param.parameters = &params[0];
	param.parameterized_k_value = 5;
	param.fixcluster0 = 3;
	param.num_lambdas = 1;

	initialize_k_weights(&param);

	DOUBLES_EQUAL(.02, weights[0], .0001);
	DOUBLES_EQUAL(.03, weights[1], .0001);
	DOUBLES_EQUAL(.04, weights[2], .0001);
	DOUBLES_EQUAL(.05, weights[3], .0001);
	DOUBLES_EQUAL(.86, weights[4], .0001);

	param.num_lambdas = 5;
	param.parameterized_k_value = 6;

	initialize_k_weights(&param);

	DOUBLES_EQUAL(.15, weights[0], .0001);
	DOUBLES_EQUAL(.16, weights[1], .0001);
	DOUBLES_EQUAL(.17, weights[2], .0001);
	DOUBLES_EQUAL(.18, weights[3], .0001);
	DOUBLES_EQUAL(.19, weights[4], .0001);
	DOUBLES_EQUAL(.15, weights[5], .0001);
}

TEST(FirstTestGroup, initialize_k_weights2)
{
	CafeParam param;
	std::vector<double> weights(10);
	std::vector<double> params(100);
	for (double i = 0; i < 100; ++i)
		params[i] = i / 100.0;
	param.k_weights = &weights[0];
	param.parameters = &params[0];
	param.parameterized_k_value = 5;
	param.fixcluster0 = 3;
	param.num_lambdas = 1;
	param.num_mus = 5;
	param.eqbg = 1;

	initialize_k_weights2(&param);

	DOUBLES_EQUAL(.1, weights[0], .0001);
	DOUBLES_EQUAL(.11, weights[1], .0001);
	DOUBLES_EQUAL(.12, weights[2], .0001);
	DOUBLES_EQUAL(.13, weights[3], .0001);
	DOUBLES_EQUAL(.54, weights[4], .0001);

	param.num_mus = 7;
	param.parameterized_k_value = 6;

	initialize_k_weights2(&param);

	DOUBLES_EQUAL(.21, weights[0], .0001);
	DOUBLES_EQUAL(.22, weights[1], .0001);
	DOUBLES_EQUAL(.23, weights[2], .0001);
	DOUBLES_EQUAL(.24, weights[3], .0001);
	DOUBLES_EQUAL(.25, weights[4], .0001);
	DOUBLES_EQUAL(-.15, weights[5], .0001);
}

TEST(PValueTests, pvalue)
{
	probability_cache = NULL;
	std::ostringstream ost;

	family_size_range range;
	range.min = 0;
	range.max = 15;
	range.root_min = 0;
	range.root_max = 15;

	print_pvalues(ost, create_tree(range), 10, 5);

	STRCMP_CONTAINS("(((chimp_1:6,human_1:6)_1:81,(mouse_1:17,rat_1:17)_1:70)_1:6,dog_1:9)_1\n", ost.str().c_str());
	STRCMP_CONTAINS("Root size: 1 with maximum likelihood : 0\n", ost.str().c_str());
	STRCMP_CONTAINS("p-value: 0\n", ost.str().c_str());
}

TEST(PValueTests, pvalues_for_family)
{
	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	family_size_range range;
	range.min = 0;
	range.max = 15;
	range.root_min = 0;
	range.root_max = 15;

	pCafeTree pcafe = create_tree(range);
	pCafeFamily pfamily = cafe_family_init(build_arraylist(species, 7));
	cafe_family_set_species_index(pfamily, pcafe);
	const char *values[] = { "description", "id", "3", "5", "7", "11", "13" };
	cafe_family_add_item(pfamily, build_arraylist(values, 7));

	ConditionalDistribution::matrix.push_back(std::vector<double>(10));

	probability_cache = birthdeath_cache_init(pcafe->size_of_factor);

	range.min = 0;
	range.max = 5;
	range.root_min = 1;
	range.root_max = 1;
	pvalues_for_family(pcafe, pfamily, &range, 1, 1, 0);

}

TEST(PValueTests, read_pvalues)
{
	std::string str("1.0\t2.0\t3.0\n1.5\t2.5\t3.5\n");
	std::istringstream stream(str);
	read_pvalues(stream, 3);
	std::vector<double> vals = ConditionalDistribution::matrix[0];
	DOUBLES_EQUAL(1.0, vals[0], .001);
	DOUBLES_EQUAL(2.0, vals[1], .001);
	DOUBLES_EQUAL(3.0, vals[2], .001);
	vals = ConditionalDistribution::matrix[1];
	DOUBLES_EQUAL(1.5, vals[0], .001);
	DOUBLES_EQUAL(2.5, vals[1], .001);
	DOUBLES_EQUAL(3.5, vals[2], .001);
}

TEST(LikelihoodRatio, cafe_likelihood_ratio_test)
{
	double *maximumPvalues = NULL;
	CafeParam param;
	param.flog = stdout;
	family_size_range range;
	range.min = range.root_min = 0;
	range.max = range.root_max = 15;
	pCafeTree tree = create_tree(range);
	param.pcafe = tree;
	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	param.pfamily = cafe_family_init(build_arraylist(species, 7));
	param.num_threads = 1;
	cafe_likelihood_ratio_test(&param, maximumPvalues);
	DOUBLES_EQUAL(0, param.likelihoodRatios[0][0], .0001);
}

TEST(LikelihoodRatio, likelihood_ratio_report)
{
	family_size_range range;
	range.min = range.root_min = 0;
	range.max = range.root_max = 15;
	pCafeTree tree = create_tree(range);
	const char *species[] = { "", "", "chimp", "human", "mouse", "rat", "dog" };
	pCafeFamily pfamily = cafe_family_init(build_arraylist(species, 7));
	const char *values[] = { "description", "family1", "3", "5", "7", "11", "13" };
	cafe_family_add_item(pfamily, build_arraylist(values, 7));
	cafe_family_set_species_index(pfamily, tree);

	std::vector<double> pvalues(2);
	pvalues[0] = 5;
	pvalues[1] = 7;

	std::vector<int> lambdas(1);
	std::vector<double*> lambda_cache(2);

	lambdas[0] = 0;
	double num = 3;
	lambda_cache[0] = &num;
	lambda_cache[1] = &num;

	char outbuf[1000];
	FILE* out = fmemopen(outbuf, 999, "w");

	likelihood_ratio_report(pfamily, tree, pvalues, lambdas, lambda_cache, out);

	fclose(out);
	STRCMP_EQUAL("family1\t(((chimp_3:6,human_5:6):81,(mouse_7:17,rat_11:17):70):6,dog_13:9)\t(0, 3.000000,0.000000)\t5\t0.025347\n", outbuf);
}

TEST(LikelihoodRatio, update_branchlength)
{
	int t = 5;
	family_size_range range;
	range.min = range.root_min = 0;
	range.max = range.root_max = 15;
	pCafeTree tree = create_tree(range);
	int *old_branchlength = (int*)memory_new(tree->super.nlist->size-1, sizeof(int));
	old_branchlength[0] = 97;

	pPhylogenyNode pnode0 = (pPhylogenyNode)tree->super.nlist->array[0];
	LONGS_EQUAL(-1, pnode0->taxaid);
	pPhylogenyNode pnode1 = (pPhylogenyNode)tree->super.nlist->array[1];
	pnode1->taxaid = 1;
	update_branchlength(tree, (pTree)tree, 1.5, old_branchlength, &t);

	DOUBLES_EQUAL(6.0, pnode0->branchlength, 0.0001);	// not updated since taxaid < 0
	DOUBLES_EQUAL(688.5, pnode1->branchlength, 0.0001);	// equation applied since taxaid > 0
	LONGS_EQUAL(6, old_branchlength[0]);		// branchlengths are copied here
}

int main(int ac, char** av)
{
	return CommandLineTestRunner::RunAllTests(ac, av);
}
