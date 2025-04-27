//
// Copyright (c) 2006-2017 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/clasp_facade.h>
#include <clasp/minimize_constraint.h>
#include <clasp/heuristics.h>
#include <clasp/lookahead.h>
#include <clasp/clingo.h>
#include <clasp/model_enumerators.h>
#include <potassco/string_convert.h>
#if CLASP_HAS_THREADS
#include <clasp/mt/mutex.h>
#endif
#include "lpcompare.h"
#include <signal.h>
#include "catch.hpp"
namespace Clasp { namespace Test {
using namespace Clasp::mt;

TEST_CASE("Facade", "[facade]") {
	Clasp::ClaspFacade libclasp;
	Clasp::ClaspConfig config;
	SECTION("with trivial program") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "a :- not b. b :- not a.");
		SECTION("testPrepareIsIdempotent") {
			libclasp.prepare();
			libclasp.prepare();
			REQUIRE(libclasp.solve().sat());
			REQUIRE(libclasp.summary().numEnum == 2);
			REQUIRE(libclasp.summary().step == 0);
		}
		SECTION("testPrepareIsImplicit") {
			REQUIRE(libclasp.solve().sat());
			REQUIRE(libclasp.summary().numEnum == 2);
			REQUIRE(libclasp.summary().step == 0);
		}
		SECTION("testPrepareSolvedProgram") {
			libclasp.prepare();
			REQUIRE(libclasp.solve().sat());
			REQUIRE(libclasp.summary().numEnum == 2);
			REQUIRE(libclasp.summary().step == 0);

			libclasp.prepare();
			REQUIRE(libclasp.solve().sat());
			REQUIRE(libclasp.summary().numEnum == 2);
			REQUIRE(libclasp.summary().step == 1);
		}
		SECTION("testSolveSolvedProgram") {
			libclasp.prepare();
			REQUIRE(libclasp.solve().sat());

			REQUIRE(libclasp.solve().sat());
			REQUIRE(libclasp.summary().numEnum == 2);
			REQUIRE(libclasp.summary().step == 1);
		}
		SECTION("testSolveAfterStopConflict") {
			struct PP : public PostPropagator {
				uint32 priority() const { return priority_reserved_msg; }
				bool propagateFixpoint(Solver& s, PostPropagator*) {
					s.setStopConflict();
					return false;
				}
			} pp;
			libclasp.ctx.master()->addPost(&pp);
			libclasp.prepare();
			REQUIRE(libclasp.solve().unknown());
			libclasp.ctx.master()->removePost(&pp);
			libclasp.update();
			REQUIRE(libclasp.solve().sat());
		}
		SECTION("testUpdateWithoutPrepareDoesNotIncStep") {
			REQUIRE(libclasp.update().ok());
			REQUIRE(libclasp.update().ok());
			libclasp.prepare();
			REQUIRE(libclasp.solve().sat());
			REQUIRE(libclasp.summary().numEnum == 2);
			REQUIRE(libclasp.summary().step == 0);
		}
		SECTION("testUpdateWithoutSolveDoesNotIncStep") {
			libclasp.prepare();
			REQUIRE(libclasp.update().ok());
			libclasp.prepare();

			REQUIRE(libclasp.solve().sat());
			REQUIRE(libclasp.summary().numEnum == 2);
			REQUIRE(libclasp.summary().step == 0);
		}
		SECTION("test interrupt") {
			struct FH : EventHandler {
				FH() : finished(0) {}
				virtual void onEvent(const Event& ev) {
					finished += event_cast<ClaspFacade::StepReady>(ev) != 0;
				}
				int finished;
			} fh;
			SECTION("interruptBeforePrepareInterruptsNext") {
				REQUIRE(libclasp.interrupt(1) == false);
				libclasp.prepare();
				REQUIRE(libclasp.solve(LitVec(), &fh).interrupted());
				REQUIRE(libclasp.solved());
				REQUIRE(fh.finished == 1);
			}
			SECTION("interruptBeforeSolveInterruptsNext") {
				libclasp.prepare();
				REQUIRE(libclasp.interrupt(1) == false);
				REQUIRE_FALSE(libclasp.solved());
				REQUIRE(libclasp.solve(LitVec(), &fh).interrupted());
				REQUIRE(libclasp.solved());
				REQUIRE(fh.finished == 1);
			}
			SECTION("interruptAfterSolveInterruptsNext") {
				libclasp.prepare();
				REQUIRE_FALSE(libclasp.solve(LitVec(), &fh).interrupted());
				REQUIRE(fh.finished == 1);
				REQUIRE(libclasp.solved());
				REQUIRE_FALSE(libclasp.interrupted());
				REQUIRE(libclasp.interrupt(1) == false);
				REQUIRE(libclasp.solve(LitVec(), &fh).interrupted());
				REQUIRE(fh.finished == 2);
			}
			SECTION("interruptBeforeUpdateInterruptsNext") {
				libclasp.prepare();
				libclasp.interrupt(1);
				libclasp.update(false);
				REQUIRE_FALSE(libclasp.interrupted());
				REQUIRE(libclasp.solve().interrupted());
			}
		}
		SECTION("testUpdateCanIgnoreQueuedSignals") {
			libclasp.prepare();
			libclasp.interrupt(1);
			libclasp.update(false, SIG_IGN);
			REQUIRE_FALSE(libclasp.solve().interrupted());
		}
		SECTION("testShutdownStopsStep") {
			libclasp.prepare();
			libclasp.shutdown();
			REQUIRE(libclasp.solved());
		}
		SECTION("testSolveUnderAssumptions") {
			Var ext = asp.newAtom();
			asp.freeze(ext, value_true);
			libclasp.prepare();
			LitVec assume(1, asp.getLiteral(1));
			struct MH : public Clasp::EventHandler {
				MH() : models(0) {}
				bool onModel(const Clasp::Solver&, const Clasp::Model& m) {
					for (LitVec::const_iterator it = exp.begin(), end = exp.end(); it != end; ++it) {
						REQUIRE(m.isTrue(*it));
					}
					++models;
					return true;
				}
				LitVec exp;
				int    models;
			} mh1, mh2;
			mh1.exp.push_back(asp.getLiteral(1));
			mh1.exp.push_back(~asp.getLiteral(2));
			mh1.exp.push_back(asp.getLiteral(ext));
			libclasp.solve(assume, &mh1);
			REQUIRE(mh1.models == 1);
			libclasp.update();
			asp.freeze(ext, value_false);
			assume.assign(1, asp.getLiteral(2));
			mh2.exp.push_back(~asp.getLiteral(1));
			mh2.exp.push_back(asp.getLiteral(2));
			mh2.exp.push_back(~asp.getLiteral(ext));
			libclasp.solve(assume, &mh2);
			REQUIRE(mh2.models == 1);
			libclasp.update();
			libclasp.solve();
			REQUIRE(libclasp.summary().numEnum == 2);
		}
	}
	SECTION("testRestartAfterPrepare") {
		libclasp.startAsp(config);
		libclasp.prepare();
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		REQUIRE_FALSE(asp.frozen());
	}

	SECTION("testUpdateChecks") {
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp, "a :- not b. b :- not a.");

		SECTION("cannotSolveAgainInSingleSolveMode") {
			REQUIRE(libclasp.solve().sat());
			REQUIRE_THROWS_AS(libclasp.prepare(), std::logic_error);
			REQUIRE_THROWS_AS(libclasp.solve(), std::logic_error);
		}

		SECTION("maySolveAgainInMultiSolveMode") {
			libclasp.ctx.setSolveMode(Clasp::SharedContext::solve_multi);
			REQUIRE(libclasp.solve().sat());
			REQUIRE_NOTHROW(libclasp.prepare());
			REQUIRE_FALSE(libclasp.solved());
			REQUIRE(libclasp.solve().sat());
		}

		SECTION("cannotUpdateInSingleShotMode") {
			Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
			libclasp.keepProgram();
			lpAdd(asp, "a :- not b. b :- not a.");
			REQUIRE(libclasp.solve().sat());
			REQUIRE_THROWS_AS(libclasp.update(), std::logic_error);
			REQUIRE_THROWS_AS(libclasp.prepare(), std::logic_error);
		}
	}

	SECTION("testPrepareTooStrongBound") {
		config.solve.numModels = 0;
		config.solve.optBound.assign(1, 0);
		lpAdd(libclasp.startAsp(config, true),
			"a :-not b.\n"
			"b :-not a.\n"
			"c.\n"
			"#minimize{c, a, b}.");

		libclasp.prepare();
		REQUIRE(libclasp.solve().unsat());
	}

	SECTION("testUnsatCore") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		int expect = 0;
		std::string prg;
		SECTION("AssumeFalse") {
			prg = "a.\n#assume{not a}.";
			expect = -1;
		}
		SECTION("AssumeTrue") {
			prg = "a :- b.\nb :- a.\n#assume{a}.";
			expect = 1;
		}
		INFO(prg);
		lpAdd(asp, prg.c_str());
		libclasp.prepare();
		REQUIRE(libclasp.solve().unsat());
		const LitVec* core = libclasp.summary().unsatCore();
		REQUIRE(core);
		CHECK(core->size() == 1);
		Potassco::LitVec out;
		CHECK(asp.extractCore(*core, out));
		CHECK(out.size() == 1);
		CHECK(out[0] == expect);
	}

	SECTION("testIssue81") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x}.\n"
		           "a :- x.\n"
		           "b :- not x.\n"
		           "#assume{a,b}.");
		libclasp.prepare();
		REQUIRE(libclasp.solve().unsat());
		const LitVec* core = libclasp.summary().unsatCore();
		REQUIRE(core);
		CHECK(core->size() == 2);
		Potassco::LitVec out;
		CHECK(asp.extractCore(*core, out));
		CHECK(out.size() == 2);
		CHECK(out[0] == 1);
		CHECK(out[1] == 2);
	}

	SECTION("testIssue84") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, false);
		lpAdd(asp,
			"d:-b.\n"
			"t|f :-c, d.\n"
			"d :-t.\n"
			"a :-c, d.\n"
			"{b;c }.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 5);
	}

	SECTION("testComputeBrave") {
		config.solve.numModels = 0;
		config.solve.enumMode = EnumOptions::enum_brave;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		std::string prg(
			"x1 :- not x2.\n"
			"x2 :- not x1.\n"
			"x3 :- not x1.\n");
		SECTION("via output") {
			prg.append("#output a : x1.\n #output b : x2.\n");
		}
		SECTION("via project") {
			prg.append("#project{x1, x2, x3}.");
		}
		lpAdd(asp, prg.c_str());
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		const Model& m = *libclasp.summary().model();
		REQUIRE(m.isDef(asp.getLiteral(1)));
		REQUIRE(m.isDef(asp.getLiteral(2)));
	}
	SECTION("testComputeQuery") {
		config.solve.numModels = 0;
		config.solve.enumMode = EnumOptions::enum_query;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp,
			"{a,b}."
			"c :- a.\n"
			"c :- b.\n"
			"c :- not a, not b.\n"
			"#output a : a.\n"
			"#output b : b.\n"
			"#output c : c.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		const Model& m = *libclasp.summary().model();
		REQUIRE(m.isDef(asp.getLiteral(3)));
		REQUIRE_FALSE(m.isDef(asp.getLiteral(1)));
		REQUIRE_FALSE(m.isDef(asp.getLiteral(2)));
	}
	SECTION("test opt enumerate") {
		config.solve.numModels = 0;
		config.solve.optMode = MinimizeMode_t::enumOpt;
		lpAdd(libclasp.startAsp(config, false),
			"{x1;x2;x3}.\n"
			":- not x1, not x2, not x3.\n"
			":- x2, not x1.\n"
			":- x3, not x1.\n"
			"#minimize{not x1}@0.\n"
			"#minimize{x1}@1.");
		libclasp.prepare();
		SECTION("with basic solve") {
			REQUIRE(libclasp.solve().sat());
			REQUIRE(uint64(4) == libclasp.summary().optimal());
		}
		SECTION("with generator") {
			unsigned num = 0, opt = 0;
			for (Clasp::ClaspFacade::SolveHandle it = libclasp.solve(SolveMode_t::Yield); it.next();) {
				++num;
				opt += it.model()->opt;
			}
			REQUIRE((num > opt && opt == 4));
		}
	}

	SECTION("testIncrementalEnum") {
		config.solve.numModels = 0;
		config.solve.enumMode  = EnumOptions::enum_record;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1}.");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 2);
		REQUIRE(libclasp.update().ok());
		lpAdd(asp, "{x2}.");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 4);
	}
	SECTION("testIncrementalCons") {
		config.solve.numModels = 0;
		config.solve.enumMode  = EnumOptions::enum_cautious;
		lpAdd(libclasp.startAsp(config, true),
			"{x1;x2;x3}.\n"
			"#output a : x1.\n"
			"#output b : x2.\n"
			"#output c : x3.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		config.solve.enumMode = EnumOptions::enum_brave;
		libclasp.update(true);
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum > 1);
	}
	SECTION("testIncrementalMin") {
		config.solve.numModels = 0;
		config.solve.enumMode  = EnumOptions::enum_auto;
		lpAdd(libclasp.startAsp(config, true),
			"{x1;x2;x3}.\n"
			"#minimize{x1, x2, x3}.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum < 8u);
		libclasp.update().ctx()->removeMinimize();
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 8);
	}
	SECTION("testIncrementalMinIgnore") {
		config.solve.optMode = MinimizeMode_t::ignore;
		config.solve.numModels = 0;
		lpAdd(libclasp.startAsp(config, true),
			"{x1;x2}.\n"
			"#minimize{x1, x2}.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 4u);
		config.solve.optMode = MinimizeMode_t::optimize;
		libclasp.update(true);
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 1u);
	}
	SECTION("testIncrementalMinAdd") {
		config.solve.numModels = 0;
		config.solve.enumMode  = EnumOptions::enum_auto;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp,
			"{x1;x2}.\n"
			"#minimize{not x1}.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().model()->isTrue(asp.getLiteral(1)));
		libclasp.update();
		lpAdd(asp, "#minimize{not x2}.");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().model()->isTrue(asp.getLiteral(1)));
		REQUIRE(libclasp.summary().model()->isTrue(asp.getLiteral(2)));
	}
	SECTION("testUncoreUndoerAssumptions") {
		config.solve.numModels = 0;
		config.solve.optMode   = MinimizeMode_t::enumOpt;
		config.addSolver(0).heuId = Heuristic_t::Domain;
		SECTION("test oll") {
			config.addSolver(0).opt.type = OptParams::type_usc;
			config.addSolver(0).opt.algo = OptParams::usc_oll;
		}
		SECTION("test one") {
			config.addSolver(0).opt.type = OptParams::type_usc;
			config.addSolver(0).opt.algo = OptParams::usc_one;
		}
		SECTION("test k") {
			config.addSolver(0).opt.type = OptParams::type_usc;
			config.addSolver(0).opt.algo = OptParams::usc_k;
		}
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp,
			"{x1;x2;x3;x4;x5}.\n"
			":- x1, x2, x3.\n"
			":- x4, x5.\n"
			":- x4, not x5.\n"
			"x5 :- not x4.\n"
			"#minimize{not x1, not x2}.\n"
			"#heuristic x4. [1,true]"
			"#assume{x3}.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numOptimal == 2);
		libclasp.update();
		libclasp.ctx.addUnary(~asp.getLiteral(3));
		libclasp.ctx.addUnary(asp.getLiteral(5));
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().costs()->at(0) == 0);
		REQUIRE(libclasp.summary().numOptimal == 1);
	}

	SECTION("testUpdateConfig") {
		config.solve.numModels = 0;
		config.solve.enumMode  = EnumOptions::enum_auto;
		config.addSolver(0).heuId  = Heuristic_t::Berkmin;
		lpAdd(libclasp.startAsp(config, true), "{x1;x2;x3}.");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		config.addSolver(0).heuId = Heuristic_t::Vsids;
		libclasp.update(true);
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(dynamic_cast<ClaspVsids*>(libclasp.ctx.master()->heuristic()));
	}
	SECTION("testIncrementalProjectUpdate") {
		config.solve.numModels = 0;
		config.solve.enumMode  = EnumOptions::enum_auto;
		config.solve.project   = 1;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1;x2}. #output b : x2.");
		libclasp.prepare();
		REQUIRE(static_cast<const ModelEnumerator*>(libclasp.enumerator())->project(asp.getLiteral(2).var()));
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 2);
		config.solve.project = 0;
		libclasp.update(true);
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 4);
		config.solve.project = 1;
		libclasp.update(true);
		lpAdd(asp, "{x3;x4}. #output y : x4.");
		libclasp.prepare();
		REQUIRE(static_cast<const ModelEnumerator*>(libclasp.enumerator())->project(asp.getLiteral(2).var()));
		REQUIRE(static_cast<const ModelEnumerator*>(libclasp.enumerator())->project(asp.getLiteral(4).var()));
		REQUIRE(libclasp.solve().sat());
		REQUIRE(uint64(4) == libclasp.summary().numEnum);
	}
	SECTION("testIncrementalDomRecUpdate") {
		config.solve.numModels = 0;
		config.solve.enumMode  = EnumOptions::enum_dom_record;
		config.addSolver(0).heuId  = Heuristic_t::Domain;
		config.addSolver(0).heuristic.domMod  = HeuParams::mod_false;
		config.addSolver(0).heuristic.domPref = HeuParams::pref_show;
		lpAdd(libclasp.startAsp(config, true),
			"{x1;x2}.\n"
			":- not x1, not x2.\n"
			"#output b : x2.\n"
			"#output a : x1.\n");
		REQUIRE(libclasp.solve().sat());
		// {a} ; {b}
		REQUIRE(libclasp.summary().numEnum == 2);

		config.addSolver(0).heuristic.domMod = HeuParams::mod_true;
		libclasp.update(true);
		REQUIRE(libclasp.solve().sat());
		// {a,b}
		REQUIRE(libclasp.summary().numEnum == 1);
	}
	SECTION("testIncrementalConfigUpdateBug") {
		config.asp.erMode = Asp::LogicProgram::mode_transform;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1;x2}.");
		libclasp.prepare();
		REQUIRE(libclasp.ctx.ok());
		REQUIRE(asp.stats.auxAtoms == 2);
		config.asp.erMode = Asp::LogicProgram::mode_native;
		libclasp.update(true);
		lpAdd(asp, "{x3;x4}.");
		libclasp.prepare();
		REQUIRE(asp.stats.auxAtoms == 0);
	}
	SECTION("with lookahead") {
		config.addSolver(0).lookType = Var_t::Atom;
		lpAdd(libclasp.startAsp(config, true), "{x1;x2}.");
		libclasp.prepare();
		REQUIRE(libclasp.ctx.master()->getPost(PostPropagator::priority_reserved_look));
		SECTION("incrementalLookaheadAddHeuristic") {
			PostPropagator* look = libclasp.ctx.master()->getPost(PostPropagator::priority_reserved_look);
			config.addSolver(0).heuId = Heuristic_t::Unit;
			libclasp.update(true);
			libclasp.prepare();
			look = libclasp.ctx.master()->getPost(PostPropagator::priority_reserved_look);
			REQUIRE((look && look->next == 0));
		}
		SECTION("incrementalDisableLookahead") {
			config.addSolver(0).lookType = 0;
			libclasp.update(true);
			libclasp.prepare();
			REQUIRE(libclasp.ctx.master()->getPost(PostPropagator::priority_reserved_look) == 0);
		}
		SECTION("incrementalChangeLookahead") {
			config.addSolver(0).lookType = Var_t::Body;
			libclasp.update(true);
			libclasp.prepare();
			Lookahead* look = static_cast<Lookahead*>(libclasp.ctx.master()->getPost(PostPropagator::priority_reserved_look));
			REQUIRE((look && look->score.types == Var_t::Body));
		}
	}
	SECTION("testIncrementalExtendLookahead") {
		config.addSolver(0).lookType = Var_t::Atom;
		config.addSolver(0).lookOps  = 3;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1;x2}.");
		libclasp.prepare();
		REQUIRE(libclasp.ctx.master()->getPost(PostPropagator::priority_reserved_look));
		config.addSolver(0).lookOps  = 0;
		libclasp.update(true);
		lpAdd(asp, "{x3;x4}.");
		libclasp.prepare();
		Lookahead* look = static_cast<Lookahead*>(libclasp.ctx.master()->getPost(PostPropagator::priority_reserved_look));
		REQUIRE((look && look->next == 0));
		while (libclasp.ctx.master()->numFreeVars() != 0) {
			libclasp.ctx.master()->decideNextBranch();
			libclasp.ctx.master()->propagate();
			REQUIRE(libclasp.ctx.master()->getPost(PostPropagator::priority_reserved_look) == look);
		}
	}

	SECTION("testIncrementalRemoveSolver") {
		config.solve.numModels = 0;
		config.solve.setSolvers(4);
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp,
			"{x1;x2;x4;x3}.\n"
			":- 3 {x1, x2, x3, x4}.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(uint64(11) == libclasp.summary().numEnum);

		config.solve.setSolvers(1);
		libclasp.update(true);
		lpAdd(asp, ":- x1, x2.\n");
		libclasp.prepare();
		REQUIRE((libclasp.solve().sat() && libclasp.summary().numEnum == 10));

		config.solve.setSolvers(2);
		libclasp.update(true);
		libclasp.prepare();
		REQUIRE((libclasp.solve().sat() && libclasp.summary().numEnum == 10));
	}

	SECTION("testGenSolveTrivialUnsat") {
		config.solve.numModels = 0;
		lpAdd(libclasp.startAsp(config, true), "x1 :- not x1.");
		libclasp.prepare();
		ClaspFacade::SolveHandle it = libclasp.solve(SolveMode_t::Yield);
		REQUIRE(it.get().exhausted());
		REQUIRE_FALSE(it.model());
	}
	SECTION("testInterruptBeforeGenSolve") {
		config.solve.numModels = 0;
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		libclasp.interrupt(2);
		ClaspFacade::SolveHandle it = libclasp.solve(SolveMode_t::Yield);
		REQUIRE(it.get().interrupted());
		REQUIRE_FALSE(it.model());
	}
	SECTION("testGenSolveWithLimit") {
		lpAdd(libclasp.startAsp(config, true), "{x1;x2;x3}.");
		libclasp.prepare();
		for (int i = 1; i != 9; ++i) {
			unsigned got = 0, exp = i;
			config.solve.numModels = i % 8;
			libclasp.update(true);
			for (ClaspFacade::SolveHandle it = libclasp.solve(SolveMode_t::Yield); it.next(); ) {
				REQUIRE(got != exp);
				++got;
			}
			REQUIRE(exp == got);
		}
	}
	SECTION("testGenSolveStartUnsat") {
		lpAdd(libclasp.startAsp(config, true), "{x1, x2}.\n :- x1, x2.\n#assume{x1, x2}.");
		libclasp.prepare();
		ClaspFacade::SolveHandle it = libclasp.solve(SolveMode_t::Yield);
		REQUIRE_FALSE(it.next());
	}

	SECTION("testCancelGenSolve") {
		config.solve.numModels = 0;
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		unsigned mod = 0;
		ClaspFacade::SolveHandle it = libclasp.solve(SolveMode_t::Yield);
		for (; it.next();) {
			REQUIRE(it.model()->num == ++mod);
			it.cancel();
			break;
		}
		REQUIRE((!libclasp.solving() && !it.get().exhausted() && mod == 1));
		libclasp.update();
		libclasp.prepare();
		mod = 0;
		for (ClaspFacade::SolveHandle j = libclasp.solve(SolveMode_t::Yield); j.next(); ++mod) {
			;
		}
		REQUIRE((!libclasp.solving() && mod == 2));
	}
	SECTION("testGenDtorCancelsSolve") {
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		{ libclasp.solve(SolveMode_t::Yield); }
		REQUIRE((!libclasp.solving() && !libclasp.result().exhausted()));
	}

	SECTION("with model handler") {
		std::string log;
		struct Handler : EventHandler {
			Handler(const char* n, std::string& l) : name(n), log(&l), doThrow(false), doStop(false) {}
			std::string name;
			std::string* log;
			bool doThrow, doStop;
			virtual bool onModel(const Solver&, const Model&) {
				log->append(!log->empty(), ' ').append(name);
				if (doThrow) { throw std::runtime_error("Model"); }
				return doStop == false;
			}
		} h1("ctx", log), h2("solve", log);
		config.solve.numModels = 0;
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		SECTION("simple") {
			h1.doStop = true;
			libclasp.ctx.setEventHandler(&h1);
			REQUIRE(libclasp.solve().sat());
			REQUIRE(log == "ctx");
		}
		SECTION("genStopFromHandler") {
			h1.doStop = true;
			libclasp.ctx.setEventHandler(&h1);
			int mod = 0;
			for (ClaspFacade::SolveHandle g = libclasp.solve(SolveMode_t::Yield, LitVec(), &h2); g.next(); ++mod) {
				log.append(" yield");
			}
			REQUIRE(mod == 1);
			REQUIRE(log == "solve ctx yield");
		}
		SECTION("syncThrowOnModel") {
			h2.doThrow = true;
			libclasp.ctx.setEventHandler(&h1);
			ClaspFacade::SolveHandle g = libclasp.solve(SolveMode_t::Yield, LitVec(), &h2);
			REQUIRE_THROWS_AS(g.model(), std::runtime_error);
			REQUIRE_FALSE(g.running());
			REQUIRE_FALSE(libclasp.solving());
			REQUIRE_THROWS_AS(g.get(), std::runtime_error);
			REQUIRE(log == "solve");
		}
	}
	SECTION("testUserConfigurator") {
		struct MyAddPost : public ClaspConfig::Configurator {
			MyAddPost() : called(false) {}
			virtual bool applyConfig(Solver&) { return called = true; }
			bool called;
		} myAddPost;
		config.addConfigurator(&myAddPost);
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		REQUIRE(myAddPost.called);
	}
	SECTION("testUserHeuristic") {
		struct MyHeu : BasicSatConfig::HeuristicCreator {
			DecisionHeuristic* create(Heuristic_t::Type, const HeuParams&) { throw MyHeu(); }
		};
		config.setHeuristicCreator(new MyHeu(), Ownership_t::Acquire);
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		REQUIRE_THROWS_AS(libclasp.prepare(), MyHeu);
	}
	SECTION("testDisposeProgram") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, false);
		lpAdd(asp,
			"{x1;x2;x3}.\n"
			"x4 :- 1 {x1, x2, x3}.\n"
			"x5 :- x1, not x2, x3.\n"
		);
		SECTION("removedByDefault") {
			libclasp.prepare();
			REQUIRE(libclasp.program() == 0);
			CHECK(!libclasp.incremental());
		}
		SECTION("kept") {
			SECTION("IfRequested") {
				libclasp.keepProgram();
				libclasp.prepare();
				CHECK(!libclasp.incremental());
			}
			SECTION("IfIncremental") {
				libclasp.enableProgramUpdates();
				libclasp.prepare();
				CHECK(libclasp.incremental());
			}
			REQUIRE(static_cast<const Clasp::Asp::LogicProgram*>(libclasp.program()) == &asp);
			CHECK(asp.getLiteral(1) == posLit(1));
			CHECK(asp.getLiteral(2) == posLit(2));
			CHECK(asp.getLiteral(3) == posLit(3));
			CHECK(asp.getLiteral(4).var() >  posLit(3).var());
		}
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 8);
	}
};

TEST_CASE("Regressions", "[facade][regression]") {
	Clasp::ClaspFacade libclasp;
	Clasp::ClaspConfig config;

	SECTION("disjunctive shifting") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp,
			"x52 :- x2.\n"
			"x2 :- x19.\n"
			"x2 :- x16.\n"
			"x6 :- x15.\n"
			"x6 :- x14.\n"
			"x6 :- x13.\n"
			"x6 :- x12.\n"
			"x19 :- x54.\n"
			"x13 :- x60.\n"
			"x12 :- x61.\n"
			"x16 :- x52.\n"
			"x12 | x13 | x14 | x15 | x16 | x19.\n"
			"x54 :- x2.\n"
			"x60 :- x6.\n"
			"x61 :- x6.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 2);
	}

	SECTION("issue 91 - 1") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp,
		  "x1 | x2.\n"
		  "x3 | x4 | x5 | x6 | x7 :- x1.\n"
		  "x1 :- x6.\n"
		  "x1 :- x7.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 6);
	}


	SECTION("issue 91 - 2") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp,
		  "a :- b.\n"
		  "b :- a.\n"
		  "c :- d.\n"
		  "d :- c.\n"
		  "f :- g.\n"
		  "g :- f.\n"
		  "c | x.\n"
		  "a | b | f | g | c | d.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 3);
	}


	SECTION("issue 91 - assertion") {
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp,
		  "a :- b.\n"
		  "b :- a.\n"
		  "f :- g.\n"
		  "g :- f.\n"
		  "b | a | g | f | c | e.\n"
		  "b | c | d.\n"
		  "b | h | e | i :-c.\n"
		  "b | a | c | d :-e.\n"
		  ":- d, e.\n");
		libclasp.prepare();
		REQUIRE(libclasp.solve().sat());
		REQUIRE(libclasp.summary().numEnum == 5);
	}
}

TEST_CASE("Incremental solving", "[facade]") {
	Clasp::ClaspFacade libclasp;
	Clasp::ClaspConfig config;
	typedef ClaspFacade::Result Result;
	Result::Base stop, done;
	int maxS = -1, minS = -1, expS = 0;
	libclasp.ctx.enableStats(2);
	config.asp.noEq();
	Asp::LogicProgram& asp = libclasp.startAsp(config, true);
	const char* prg[] = {
		// step 0
		"x1 :- x2.\n"
		"x2 :- x1.\n"
		"x1 :- x3.\n"
		":- not x1.\n"
		"#external x3."
		, // step 1
		"x3 :- x4.\n"
		"x4 :- x3.\n"
		"x4 :- x5.\n"
		"#external x5."
		, // step 2
		"x5 :- x6, x7.\n"
		"x6 :- not x3.\n"
		"x7 :- not x1, not x2.\n"
		"{x5}."
		, // step 3
		"{x8}."
	};
	SECTION("test stop on sat - no limit") {
		stop = done = Result::SAT;
		expS = 2;
	}
	SECTION("test stop on unsat - no limit") {
		stop = done = Result::UNSAT;
	}
	SECTION("test stop on sat - with max step") {
		stop = Result::SAT;
		done = Result::UNSAT;
		maxS = 2;
		expS = 1;
	}
	SECTION("test stop on sat - with min step") {
		stop = Result::SAT;
		done = Result::SAT;
		minS = 4;
		expS = 3;
	}
	Result::Base res = Result::UNKNOWN;
	do {
		libclasp.update();
		REQUIRE(std::size_t(libclasp.step()) < (sizeof(prg)/sizeof(const char*)));
		lpAdd(asp, prg[libclasp.step()]);
		libclasp.prepare();
		res = libclasp.solve();
		if (res == Result::UNSAT) {
			int expCore = libclasp.step() == 0 ? -3 : -5;
			Potassco::LitVec prgCore;
			const LitVec* core = libclasp.summary().unsatCore();
			REQUIRE(core);
			CHECK(asp.extractCore(*core, prgCore));
			CHECK(prgCore.size() == 1);
			CHECK(prgCore[0] == expCore);
		}
	} while (--maxS && ((minS > 0 && --minS) || res != stop));
	REQUIRE(done == (Result::Base)libclasp.result());
	REQUIRE(expS == libclasp.step());
}

#if CLASP_HAS_THREADS

TEST_CASE("Facade mt", "[facade][mt]") {
	struct EventVar {
		EventVar() : fired(false) {}
		void fire() {
			{
				Clasp::mt::unique_lock<Clasp::mt::mutex> lock(mutex);
				fired = true;
			}
			cond.notify_all();
		}
		void wait() {
			for (Clasp::mt::unique_lock<Clasp::mt::mutex> lock(mutex); !fired;) {
				cond.wait(lock);
			}
		}
		Clasp::mt::mutex mutex;
		Clasp::mt::condition_variable cond;
		bool fired;
	};

	struct ModelHandler : EventHandler {
		ModelHandler(const char* n , std::string& l, bool r = true)
			: name(n)
			, log(&l)
			, ret(r)
		{}

		virtual bool onModel(const Solver&, const Model&) {
			log->append(!log->empty(), ' ').append(name);
			return ret;
		}
		std::string  name;
		std::string* log;
		bool         ret;
	};

	Clasp::ClaspFacade libclasp;
	Clasp::ClaspConfig config;
	typedef ClaspFacade::SolveHandle AsyncResult;
	SECTION("testIncrementalAddSolver") {
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1;x2}.");
		libclasp.prepare();
		REQUIRE_FALSE(isSentinel(libclasp.ctx.stepLiteral()));
		config.solve.setSolvers(2);
		libclasp.update(true);
		lpAdd(asp, "{x3;x4}.");
		libclasp.prepare();
		REQUIRE((libclasp.ctx.concurrency() == 2 && libclasp.ctx.hasSolver(1)));
	}
	SECTION("testClingoSolverStatsRemainValid") {
		config.stats = 2;
		config.solve.algorithm.threads = 2;
		config.solve.numModels = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1,x2,x3}.");
		libclasp.prepare();
		libclasp.solve();
		typedef Potassco::AbstractStatistics::Key_t Key_t;
		Potassco::AbstractStatistics* stats = libclasp.getStats();
		Key_t s = stats->get(stats->root(), "solving.solver");
		Key_t s1 = stats->at(s, 1);
		Key_t s1c = stats->get(stats->at(s, 1), "choices");
		Key_t s0c = stats->get(stats->at(s, 0), "choices");
		REQUIRE(stats->size(s) == 2);
		REQUIRE(stats->value(s1c) + stats->value(s0c) == stats->value(stats->get(stats->root(), "solving.solvers.choices")));
		config.solve.algorithm.threads = 1;
		libclasp.update(true);
		libclasp.solve();
		INFO("solver stats are not removed");
		REQUIRE(stats->size(s) == 2);
		INFO("solver stats remain valid");
		REQUIRE(stats->at(s, 1) == s1);
		REQUIRE(stats->value(s1c) == 0.0);
		REQUIRE(stats->value(s0c) == stats->value(stats->get(stats->root(), "solving.solvers.choices")));
		config.solve.algorithm.threads = 2;
		libclasp.update(true);
		libclasp.solve();
		REQUIRE(stats->value(s1c) + stats->value(s0c) == stats->value(stats->get(stats->root(), "solving.solvers.choices")));
	}
	SECTION("testShareModeRegression") {
		config.shareMode = ContextParams::share_auto;
		config.solve.algorithm.threads = 2;
		libclasp.startSat(config).prepareProblem(2);
		libclasp.prepare();
		REQUIRE(libclasp.ctx.physicalShare(Constraint_t::Static));
		REQUIRE(libclasp.ctx.physicalShare(Constraint_t::Conflict));
	}
	SECTION("testAsyncSolveTrivialUnsat") {
		config.solve.numModels = 0;
		lpAdd(libclasp.startAsp(config, true), "x1 :- not x1.");
		libclasp.prepare();
		AsyncResult it = libclasp.solve(SolveMode_t::Async|SolveMode_t::Yield);
		REQUIRE(it.get().unsat());
	}
	SECTION("testSolveWinnerMt") {
		struct Blocker : public PostPropagator {
			Blocker(EventVar& ev) : eventVar(&ev) {}
			uint32 priority() const { return PostPropagator::priority_reserved_ufs; }
			virtual bool propagateFixpoint(Solver&, Clasp::PostPropagator*) { return true; }
			virtual bool isModel(Solver&) { eventVar->wait(); return true; }
			EventVar* eventVar;
		};
		struct Unblocker : EventHandler {
			Unblocker(EventVar& ev) : eventVar(&ev) {}
			virtual bool onModel(const Solver&, const Model&) { eventVar->fire(); return false; }
			EventVar* eventVar;
		};
		config.solve.numModels = 0;
		config.solve.enumMode = EnumOptions::enum_record;
		config.solve.algorithm.threads = 4;
		lpAdd(libclasp.startAsp(config, true), "{x1;x2;x3;x4}.");
		EventVar eventVar;
		libclasp.prepare();
		uint32 expectedWinner = 0;
		SECTION("Solver 3") { expectedWinner = 3; }
		SECTION("Solver 1") { expectedWinner = 1; }
		SECTION("Solver 2") { expectedWinner = 2; }
		SECTION("Solver 0") { expectedWinner = 0; }
		for (uint32 i = 0; i != libclasp.ctx.concurrency(); ++i) {
			if (i != expectedWinner)
				libclasp.ctx.solver(i)->addPost(new Blocker(eventVar));
		}
		Unblocker unblocker(eventVar);
		AsyncResult result = libclasp.solve(SolveMode_t::Async, LitVec(), &unblocker);
		uint32 winner = result.waitFor(5.0) ? libclasp.ctx.winner() : (eventVar.fire(), result.wait(), 0xFEE1DEAD);
		REQUIRE(winner == expectedWinner);
	}
	SECTION("testInterruptBeforeSolve") {
		config.solve.numModels = 0;
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		libclasp.interrupt(2);
		AsyncResult it = libclasp.solve(SolveMode_t::AsyncYield);
		REQUIRE(it.get().interrupted());
	}
	SECTION("testCancelAsyncOperation") {
		config.solve.numModels = 0;
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		AsyncResult it = libclasp.solve(SolveMode_t::AsyncYield);
		while (it.model()) {
			it.cancel();
		}
		REQUIRE(uint64(1) == libclasp.summary().numEnum);
		REQUIRE((!libclasp.solving() && it.interrupted()));
		libclasp.update();
		libclasp.prepare();

		std::string log;
		ModelHandler eh1("ctx", log);
		ModelHandler eh2("solve", log);

		libclasp.ctx.setEventHandler(&eh1);
		it = libclasp.solve(SolveMode_t::AsyncYield, {}, &eh2);
		int mod = 0;

		while (it.model()) {
			log.append(" yield");
			++mod;
			it.resume();
		}
		REQUIRE((!libclasp.solving() && mod == 2));
		REQUIRE(log == "solve ctx yield solve ctx yield");
	}
	SECTION("testAsyncResultDtorCancelsOp") {
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		{ AsyncResult it = libclasp.solve(SolveMode_t::AsyncYield); }
		REQUIRE((!libclasp.solving() && libclasp.result().interrupted()));
	}

	SECTION("testDestroyAsyncResultNoFacade") {
		{
			ClaspFacade* localLib = new ClaspFacade();
			lpAdd(localLib->startAsp(config, true), "{x1}.");
			localLib->prepare();
			AsyncResult res(localLib->solve(SolveMode_t::AsyncYield));
			delete localLib;
			REQUIRE(res.interrupted());
		}
	}
	SECTION("testDestroyDanglingAsyncResult") {
		AsyncResult* handle = 0;
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		handle = new AsyncResult(libclasp.solve(SolveMode_t::Async));
		handle->wait();
		libclasp.update();
		libclasp.prepare();
		AsyncResult* it = new AsyncResult(libclasp.solve(SolveMode_t::AsyncYield));
		delete handle;
		REQUIRE((!it->interrupted() && libclasp.solving()));
		REQUIRE_NOTHROW(delete it);
		REQUIRE_FALSE(libclasp.solving());
	}
	SECTION("testCancelDanglingAsyncOperation") {
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		AsyncResult step0 = libclasp.solve(SolveMode_t::Async);
		step0.wait();
		libclasp.update();
		libclasp.prepare();
		AsyncResult step1 = libclasp.solve(SolveMode_t::AsyncYield);

		step0.cancel();
		REQUIRE(libclasp.solving());
		step1.cancel();
		REQUIRE_FALSE(libclasp.solving());
	}
	SECTION("testGenSolveMt") {
		config.solve.numModels = 0;
		config.solve.algorithm.threads = 2;
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		int mod = 0;
		std::string log;
		ModelHandler eh1("ctx", log);
		ModelHandler eh2("solve", log);

		libclasp.ctx.setEventHandler(&eh1);
		for (ClaspFacade::SolveHandle it = libclasp.solve(SolveMode_t::Yield, {}, &eh2); it.next(); ++mod) {
			log.append(" yield");
		}
		REQUIRE((!libclasp.solving() && mod == 2));
		REQUIRE(log == "solve ctx yield solve ctx yield");
	}
	SECTION("test async throw") {
		struct Handler : EventHandler {
			Handler() : throwModel(false), throwFinish(false) {}
			bool throwModel, throwFinish;
			virtual bool onModel(const Solver&, const Model&) {
				if (throwModel) { throw std::runtime_error("Model"); }
				return true;
			}
			virtual void onEvent(const Event& ev) {
				if (event_cast<ClaspFacade::StepReady>(ev) && throwFinish) {
					throw std::runtime_error("Finish");
				}
			}
		} h;
		lpAdd(libclasp.startAsp(config, true), "{x1}.");
		libclasp.prepare();
		SECTION("on model") {
			h.throwModel = true;
			AsyncResult step0 = libclasp.solve(SolveMode_t::Async, LitVec(), &h);
			step0.wait();
			REQUIRE(step0.error());
			REQUIRE_THROWS_AS(step0.get(), std::runtime_error);
		}
		SECTION("on finish") {
			h.throwFinish = true;
			AsyncResult step0 = libclasp.solve(SolveMode_t::Async, LitVec(), &h);
			step0.wait();
			REQUIRE(step0.error());
			REQUIRE_THROWS_AS(step0.get(), std::runtime_error);
		}
	}
	SECTION("test mt exception handling") {
		EventVar ev;
		config.solve.numModels = 0;
		config.solve.setSolvers(2);
		lpAdd(libclasp.startAsp(config, true), "{x1;x2}.");
		libclasp.prepare();
		SECTION("throwOnModel") {
			struct Blocker : public PostPropagator {
				explicit Blocker(EventVar& e) : ev(&e) {}
				uint32 priority() const { return PostPropagator::priority_reserved_ufs + 10; }
				bool   propagateFixpoint(Solver& s, Clasp::PostPropagator* ctx) {
					if (!ctx && s.numFreeVars() == 0) {
						ev->wait();
					}
					return true;
				}
				EventVar* ev;
			};
			libclasp.ctx.master()->addPost(new Blocker(ev));
			struct Handler : EventHandler {
				virtual bool onModel(const Solver& s, const Model&) {
					if (&s != s.sharedContext()->master()) {
						ev->fire();
						throw std::runtime_error("Model from thread");
					}
					return false;
				}
				EventVar* ev;
			} h; h.ev = &ev;
			REQUIRE_THROWS_AS(libclasp.solve(SolveMode_t::Default, LitVec(), &h), std::runtime_error);
		}
		SECTION("throw on propagate") {
			struct Blocker : public PostPropagator {
				enum ET { none, alloc, logic };
				explicit Blocker(EventVar& e, ET t) : ev(&e), et(t) {}
				uint32 priority() const { return PostPropagator::priority_reserved_ufs + 10; }
				bool   propagateFixpoint(Solver& s, Clasp::PostPropagator*) {
					if (et == none) {
						ev->wait();
						s.removePost(this);
						delete this;
					}
					else {
						ev->fire();
						if (et == alloc) { throw std::bad_alloc(); }
						else             { throw std::logic_error("Something happend"); }
					}
					return true;
				}
				EventVar* ev;
				ET        et;
			};
			libclasp.ctx.master()->addPost(new Blocker(ev, Blocker::none));
			SECTION("allocFailContinue") {
				libclasp.ctx.solver(1)->addPost(new Blocker(ev, Blocker::alloc));
				REQUIRE_NOTHROW(libclasp.solve());
				REQUIRE(libclasp.summary().numEnum == 4);
			}
			SECTION("logicFailStop") {
				libclasp.ctx.solver(1)->addPost(new Blocker(ev, Blocker::logic));
				REQUIRE_THROWS_AS(libclasp.solve(), std::logic_error);
			}
		}
	}
	SECTION("Parallel solve calls clingo total check twice if necessary") {
		class Prop : public Potassco::AbstractPropagator {
		public:
			Prop() : bound(2) {}
			virtual void propagate(Potassco::AbstractSolver& s, const ChangeList&)  {
				if (s.id() == 1)
					waitFor0.wait(); // wait until Solver 0 has found its first total assignment
			}
			virtual void undo(const Potassco::AbstractSolver&, const ChangeList&) {}
			virtual void check(Potassco::AbstractSolver& s) {
				// Solver 0 enters first with |B| = 1 < bound but then waits for Solver 1
				// Solver 1 enters with |B| = 0 and notifies Solver 0 once the model is committed
				// Solver 0 is forced to enter check() again with |B| = 1 and discards this now worse assignment
				Potassco::LitVec B;
				if (s.assignment().isTrue(lit))
					B.push_back(-lit);

				std::unique_lock<std::mutex> lock(m);
				if (B.size() < bound)  { bound = B.size(); }
				else                   { s.addClause(Potassco::toSpan(B)); }
				lock.unlock();
				if (s.id() == 0) {
					waitFor0.fire(); // let Solver 1 continue
					waitFor1.wait(); // wait for Solver 1 to commit its model
				}
			}
			EventVar waitFor1;
			EventVar waitFor0;
			std::mutex m;
			int bound;
			Potassco::Lit_t lit;
		} prop;
		ClingoPropagatorInit pp(prop);
		config.addConfigurator(&pp);
		EventVar ev;
		config.solve.numModels = 0;
		config.solve.enumMode = EnumOptions::enum_record;
		config.solve.setSolvers(2);
		config.addSolver(0).signDef = SolverStrategies::sign_pos; // assume x1  -> bound = 1
		config.addSolver(1).signDef = SolverStrategies::sign_neg; // assume ~x1 -> bound = 0
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1}.");
		asp.endProgram();
		Clasp::Literal l1 = asp.getAtom(1)->literal();
		prop.lit = pp.addWatch(l1);
		pp.addWatch(~l1);

		struct Handler : EventHandler {
			Handler() {}
			virtual bool onModel(const Solver&, const Model&) {
				prop->waitFor0.fired = false; // ensure that we wait again on next propagate and
				prop->waitFor1.fire();        // wake up Solver 0
				return true;
			}
			Prop* prop;
		} h;
		h.prop = &prop;
		libclasp.solve(&h);
		REQUIRE(libclasp.summary().numEnum == 1);
	}
}

#endif

static void getStatsKeys(const Potassco::AbstractStatistics& stats, Potassco::AbstractStatistics::Key_t k, std::vector<std::string>& out, const std::string& p) {
	if (stats.type(k) == Potassco::Statistics_t::Map) {
		for (uint32 i = 0, end = stats.size(k); i != end; ++i) {
			const char* sk = stats.key(k, i);
			getStatsKeys(stats, stats.get(k, sk), out, p.empty() ? sk : std::string(p).append(".").append(sk));
		}
	}
	else if (stats.type(k) == Potassco::Statistics_t::Array) {
		for (uint32 i = 0, end = stats.size(k); i != end; ++i) {
			getStatsKeys(stats, stats.at(k, i), out, std::string(p).append(".").append(Potassco::StringBuilder().appendFormat("%d", i).c_str()));
		}
	}
	else {
		out.push_back(p);
	}
}

static void addExternalStats(Potassco::AbstractStatistics* us, Potassco::AbstractStatistics::Key_t userRoot) {
	typedef Potassco::AbstractStatistics::Key_t Key_t;

	Key_t general = us->add(userRoot, "deathCounter", Potassco::Statistics_t::Map);
	REQUIRE(us->get(userRoot, "deathCounter") == general);
	REQUIRE(us->type(general) == Potassco::Statistics_t::Map);
	Key_t value   = us->add(general, "total", Potassco::Statistics_t::Value);
	us->set(value, 42.0);
	value = us->add(general, "chickens", Potassco::Statistics_t::Value);
	us->set(value, 712.0);

	Key_t array = us->add(general, "thread", Potassco::Statistics_t::Array);
	REQUIRE(us->get(general, "thread") == array);
	REQUIRE(us->type(array) == Potassco::Statistics_t::Array);
	REQUIRE(us->size(array) == 0);
	for (size_t t = 0; t != 4; ++t) {
		Key_t a = us->push(array, Potassco::Statistics_t::Map);
		value   = us->add(a, "total", Potassco::Statistics_t::Value);
		us->set(value, 20*(t+1));
		Key_t m = us->add(a, "Animals", Potassco::Statistics_t::Map);
		value   = us->add(m, "chicken", Potassco::Statistics_t::Value);
		us->set(value, 2*(t+1));
		value   = us->add(m, "cows", Potassco::Statistics_t::Value);
		us->set(value, 5*(t+1));
		value   = us->add(a, "feeding cost", Potassco::Statistics_t::Value);
		us->set(value, t+1);
	}
	REQUIRE(us->add(userRoot, "deathCounter", Potassco::Statistics_t::Map) == general);
}

TEST_CASE("Facade statistics", "[facade]") {
	Clasp::ClaspFacade libclasp;
	Clasp::ClaspConfig config;
	config.stats = 2;
	config.solve.numModels = 0;
	SECTION("testClingoStats") {
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1;x2;x3}. x4. #minimize{x1, x2, x4}.");
		libclasp.prepare();
		libclasp.solve();
		Potassco::AbstractStatistics* stats = libclasp.getStats();
		typedef Potassco::AbstractStatistics::Key_t Key_t;
		Key_t r = stats->root();
		REQUIRE(stats->type(r) == Potassco::Statistics_t::Map);
		REQUIRE(stats->writable(r) == true);
		Key_t lp = stats->get(r, "problem.lp");
		REQUIRE(stats->writable(lp) == false);

		Key_t s = stats->get(r, "solving");
		Key_t m = stats->get(r, "summary.models");
		REQUIRE(stats->type(lp) == Potassco::Statistics_t::Map);
		REQUIRE(stats->value(stats->get(lp, "rules")) == double(asp.stats.rules[0].sum()));
		REQUIRE(stats->value(stats->get(m, "enumerated")) == double(libclasp.summary().numEnum));
		Key_t solvers = stats->get(s, "solvers");
		REQUIRE(stats->value(stats->get(solvers, "choices")) == double(libclasp.ctx.master()->stats.choices));
		Key_t costs = stats->get(r, "summary.costs");
		REQUIRE(stats->type(costs) == Potassco::Statistics_t::Array);
		REQUIRE(stats->value(stats->at(costs, 0)) == double(libclasp.summary().costs()->at(0)));

		Key_t lower = stats->get(r, "summary.lower");
		REQUIRE(stats->type(lower) == Potassco::Statistics_t::Array);
		REQUIRE(stats->size(lower) == 1);
		REQUIRE(stats->value(stats->at(lower, 0)) == libclasp.enumerator()->minimizer()->lower(0) + libclasp.enumerator()->minimizer()->adjust(0));

		Key_t solver = stats->get(s, "solver");
		REQUIRE(stats->type(solver) == Potassco::Statistics_t::Array);
		Key_t s0 = stats->at(solver, 0);
		REQUIRE(stats->type(s0) == Potassco::Statistics_t::Map);
		REQUIRE(stats->value(stats->get(s0, "choices")) == double(libclasp.ctx.master()->stats.choices));
		std::vector<std::string> keys;
		getStatsKeys(*stats, r, keys, "");
		REQUIRE_FALSE(keys.empty());
		for (std::vector<std::string>::const_iterator it = keys.begin(), end = keys.end(); it != end; ++it) {
			Key_t result;
			REQUIRE(stats->find(r, it->c_str(), &result));
			REQUIRE(result == stats->get(r, it->c_str()));
			REQUIRE(stats->type(result) == Potassco::Statistics_t::Value);
		}
		REQUIRE(keys.size() == 238);

		Key_t result;
		REQUIRE(stats->find(r, "problem.lp", &result));
		REQUIRE(result == lp);
		REQUIRE_FALSE(stats->find(lp, "foo", 0));
		REQUIRE(stats->find(lp, "rules", &result));
	}
	SECTION("testClingoStatsKeyIntegrity") {
		config.addTesterConfig()->stats = 2;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1;x2;x3}. #minimize{x1, x2}.");
		libclasp.prepare();
		libclasp.solve();
		typedef Potassco::AbstractStatistics::Key_t Key_t;
		Potassco::AbstractStatistics* stats = libclasp.getStats();
		Key_t lp = stats->get(stats->root(), "problem.lp");
		Key_t sccs = stats->get(lp, "sccs");
		Key_t m0 = stats->get(stats->root(), "summary.costs.0");
		Key_t l0 = stats->get(stats->root(), "summary.lower.0");
		REQUIRE_THROWS_AS(stats->get(stats->root(), "hcc"), std::logic_error);
		REQUIRE(stats->value(m0) == (double)libclasp.summary().costs()->at(0));
		REQUIRE(stats->value(l0) == 0);
		libclasp.update();
		lpAdd(asp,
			"x4 | x5 :- x6, not x1."
			"x6 :- x4, x5, not x2."
			"x6 :- not x1."
			);
		libclasp.prepare();
		libclasp.solve();
		REQUIRE(asp.stats.sccs == 1);
		REQUIRE(asp.stats.nonHcfs == 1);
		REQUIRE(lp == stats->get(stats->root(), "problem.lp"));
		REQUIRE(sccs == stats->get(lp, "sccs"));
		REQUIRE(m0 == stats->get(stats->root(), "summary.costs.0"));
		REQUIRE(l0 == stats->get(stats->root(), "summary.lower.0"));
		REQUIRE(stats->value(sccs) == asp.stats.sccs);
		REQUIRE(stats->value(m0) == (double)libclasp.summary().costs()->at(0));
		Key_t hcc0 = stats->get(stats->root(), "problem.hcc.0");
		Key_t hcc0Vars = stats->get(hcc0, "vars");
		REQUIRE(stats->value(hcc0Vars) != 0.0);
		libclasp.update();
		libclasp.ctx.removeMinimize();
		lpAdd(asp,
			"x7 | x8 :- x9, not x1."
			"x9 :- x7, x8, not x2."
			"x9 :- not x1."
			);
		libclasp.prepare();
		libclasp.solve();
		REQUIRE(libclasp.summary().lpStats()->sccs == 2);
		REQUIRE(libclasp.summary().lpStats()->nonHcfs == 2);
		REQUIRE(lp == stats->get(stats->root(), "problem.lp"));
		REQUIRE(sccs == stats->get(lp, "sccs"));
		REQUIRE_THROWS_AS(stats->value(m0), std::logic_error);
		REQUIRE_THROWS_AS(stats->value(l0), std::logic_error);
		REQUIRE(stats->value(hcc0Vars) != 0.0);
		REQUIRE(stats->value(stats->get(stats->root(), "problem.hcc.1.vars")) != 0.0);
	}
	SECTION("testClingoStatsWithoutStats") {
		config.stats = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp,
			"{x1,x2,x3}."
			"x3 | x4 :- x5, not x1."
			"x5 :- x4, x3, not x2."
			"x5 :- not x1."
			);
		libclasp.solve();
		typedef Potassco::AbstractStatistics::Key_t Key_t;
		Potassco::AbstractStatistics* stats = libclasp.getStats();
		Key_t root = stats->root();
		REQUIRE(stats->size(root) == 3);
		REQUIRE(stats->get(root, "solving") != root);
		REQUIRE(stats->get(root, "problem") != root);
		REQUIRE(stats->get(root, "summary") != root);
		REQUIRE_THROWS_AS(stats->get(root, "solving.accu"), std::out_of_range);
		Key_t solving = stats->get(root, "solving");
		REQUIRE(stats->find(solving, "accu", 0) == false);
	}
	SECTION("testClingoStatsBug") {
		config.stats = 0;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x2,x3}. #minimize{not x1,x2}.");
		libclasp.solve();
		Potassco::AbstractStatistics* stats = libclasp.getStats();
		typedef Potassco::AbstractStatistics::Key_t Key_t;
		Key_t root = stats->root();
		Key_t costs, minVal;
		REQUIRE(stats->size(root) == 3);
		REQUIRE((costs = stats->get(root, "summary.costs")) != root);
		REQUIRE(stats->type(costs) == Potassco::Statistics_t::Array);
		REQUIRE(stats->size(costs) == 1);
		REQUIRE((minVal = stats->get(root, "summary.costs.0")) != root);
		REQUIRE(stats->type(minVal) == Potassco::Statistics_t::Value);
		config.solve.numModels = -1;
		libclasp.update(true);
		lpAdd(asp, ":- not x1.");
		libclasp.solve();
		REQUIRE(stats->type(costs) == Potassco::Statistics_t::Array);
		REQUIRE(stats->size(costs) == 0);
		REQUIRE_THROWS_AS(stats->value(minVal), std::logic_error);
	}
	SECTION("testWritableStats") {
		ClaspStatistics stats;
		typedef ClaspStatistics::Key_t Key_t;
		StatsMap* rootMap = stats.makeRoot();
		double v1 = 2.0;
		rootMap->add("fixed", StatisticObject::value(&v1));

		Key_t root = stats.root();
		REQUIRE(stats.writable(root));
		REQUIRE(stats.writable(stats.get(root, "fixed")) == false);

		Key_t v2 = stats.add(root, "mutable", Potassco::Statistics_t::Value);
		REQUIRE(stats.writable(v2));
		stats.set(v2, 22.0);
		REQUIRE(stats.value(v2) == 22.0);
		Key_t found;
		REQUIRE(stats.find(root, "mutable", &found));
		REQUIRE(found == v2);

		Key_t arr = stats.add(root, "array", Potassco::Statistics_t::Array);
		REQUIRE(stats.type(arr) == Potassco::Statistics_t::Array);
		REQUIRE(stats.writable(arr));
		REQUIRE(stats.size(arr) == 0);

		Key_t mapAtArr0 = stats.push(arr, Potassco::Statistics_t::Map);
		REQUIRE(stats.type(mapAtArr0) == Potassco::Statistics_t::Map);
		REQUIRE(stats.size(arr) == 1);
	}
	SECTION("testClingoUserStats") {
		typedef Potassco::AbstractStatistics::Key_t Key_t;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1;x2;x3}. #minimize{x1, x2}.");
		libclasp.prepare();
		libclasp.solve();
		Potassco::AbstractStatistics* stats = libclasp.getStats();

		Key_t r = stats->root();
		REQUIRE(stats->type(r) == Potassco::Statistics_t::Map);

		Key_t u = stats->add(r, "user_step", Potassco::Statistics_t::Map);
		addExternalStats(stats, u);

		REQUIRE(stats->type(u) == Potassco::Statistics_t::Map);
		Key_t user = stats->get(u, "deathCounter");
		REQUIRE(stats->type(user) == Potassco::Statistics_t::Map);
		REQUIRE(stats->value(stats->get(user, "total")) == double(42));
		REQUIRE(stats->value(stats->get(user, "chickens")) == double(712));
		Key_t array = stats->get(user, "thread");
		REQUIRE(stats->type(array) == Potassco::Statistics_t::Array);
		REQUIRE(stats->size(array) == 4);
		for (size_t t = 0; t != 4; ++t) {
			Key_t m1 = stats->at(array, t);
			REQUIRE(stats->type(m1) == Potassco::Statistics_t::Map);
			Key_t value = stats->get(m1, "total");
			REQUIRE(stats->type(value) == Potassco::Statistics_t::Value);
			REQUIRE(stats->value(value) == double(20*(t+1)));
			Key_t m2 = stats->get(m1, "Animals");
			REQUIRE(stats->type(m2) == Potassco::Statistics_t::Map);
			value = stats->get(m2, "chicken");
			REQUIRE(stats->value(value) == double(2*(t+1)));
			value = stats->get(m2, "cows");
			REQUIRE(stats->value(value) == double(5*(t+1)));
			value = stats->get(m1, "feeding cost");
			REQUIRE(stats->value(value) == double(t+1));
		}
		Key_t total;
		REQUIRE(stats->find(r, "user_step.deathCounter.thread.1.total", &total));
		REQUIRE(stats->type(total) == Potassco::Statistics_t::Value);
		REQUIRE(stats->value(total) == 40.0);
		REQUIRE_FALSE(stats->find(r, "user_step.deathCounter.thread.5.total", 0));

		std::vector<std::string> keys;
		getStatsKeys(*stats, r, keys, "");
		REQUIRE_FALSE(keys.empty());
		for (std::vector<std::string>::const_iterator it = keys.begin(), end = keys.end(); it != end; ++it) {
			REQUIRE(stats->find(r, it->c_str(), 0));
			REQUIRE(stats->type(stats->get(r, it->c_str())) == Potassco::Statistics_t::Value);
		}
		REQUIRE(keys.size() == 256);
	}
}
namespace {
class MyProp : public Potassco::AbstractPropagator {
public:
	MyProp() : fire(lit_false()), clProp(Potassco::Clause_t::Learnt) {}
	virtual void propagate(Potassco::AbstractSolver& s, const ChangeList& changes) {
		map(changes);
		addClause(s);
	}
	virtual void undo(const Potassco::AbstractSolver&, const ChangeList& changes) {
		map(changes);
	}
	virtual void check(Potassco::AbstractSolver& s) {
		const Potassco::AbstractAssignment& assign = s.assignment();
		for (Potassco::LitVec::const_iterator it = clause.begin(), end = clause.end(); it != end; ++it) {
			if (assign.isTrue(*it)) { return; }
		}
		if (!clause.empty()) { s.addClause(Potassco::toSpan(clause)); }
	}
	void map(const ChangeList& changes) {
		change.clear();
		for (const Potassco::Lit_t* x = Potassco::begin(changes); x != Potassco::end(changes); ++x) {
			change.push_back(decodeLit(*x));
		}
	}
	bool addClause(Potassco::AbstractSolver& s) {
		if (!s.assignment().isTrue(encodeLit(fire))) {
			return true;
		}
		return s.addClause(Potassco::toSpan(clause), clProp) && s.propagate();
	}
	void addToClause(Literal x) {
		clause.push_back(encodeLit(x));
	}
	Literal  fire;
	LitVec change;
	Potassco::LitVec   clause;
	Potassco::Clause_t clProp;
};

struct PropagatorTest {
	void addVars(int num) {
		v.resize(num + 1);
		v[0] = 0;
		for (int i = 1; i <= num; ++i) {
			v[i] = ctx.addVar(Var_t::Atom);
		}
		ctx.startAddConstraints();
	}
	bool isFrozen(Var v) const {
		return ctx.varInfo(v).frozen();
	}
	bool isFrozen(Literal l) const {
		return isFrozen(l.var());
	}
	SharedContext ctx;
	VarVec v;
};

struct DebugLock : ClingoPropagatorLock {
	DebugLock() : locked(false) {}
	virtual void lock() {
		REQUIRE_FALSE(locked);
		locked = true;
	}
	virtual void unlock() {
		REQUIRE(locked);
		locked = false;
	}
	bool locked;
};

}
TEST_CASE("Clingo propagator", "[facade][propagator]") {
	typedef ClingoPropagatorInit MyInit;
	PropagatorTest test;
	SharedContext& ctx = test.ctx;
	VarVec&        v = test.v;
	MyProp         prop;
	MyInit         tp(prop);

	SECTION("testAssignmentBasics") {
		ClingoAssignment assignment(*ctx.master());

		REQUIRE(assignment.size() == 1);
		REQUIRE(assignment.trailSize() == 1);
		REQUIRE(assignment.trailBegin(0) == 0);
		REQUIRE(assignment.trailAt(0) == encodeLit(lit_true()));
		REQUIRE(assignment.trailEnd(0) == 1);

		test.addVars(2);
		const Potassco::Atom_t a1 = Potassco::atom(encodeLit(posLit(v[1])));
		const Potassco::Atom_t a2 = Potassco::atom(encodeLit(posLit(v[2])));
		REQUIRE(assignment.size() == 3);
		REQUIRE(assignment.level(a1) == UINT32_MAX);
		REQUIRE(assignment.level(a2) == UINT32_MAX);

		ctx.requestStepVar();
		REQUIRE(assignment.size() == 3);
		ctx.endInit();
		REQUIRE(assignment.size() == 4);
		REQUIRE(assignment.trailSize() == 1);

		Solver& master = *ctx.master();
		master.pushRoot(ctx.stepLiteral());
		REQUIRE(assignment.trailSize() == 2);
		REQUIRE(assignment.trailBegin(1) == 1);
		REQUIRE(assignment.trailAt(1) == encodeLit(ctx.stepLiteral()));
		REQUIRE(assignment.trailEnd(1) == 2);

		master.assume(posLit(v[1])) && master.propagate();
		master.assume(negLit(v[2])) && master.propagate();

		REQUIRE(assignment.isTotal());
		REQUIRE(assignment.trailSize() == 4);
		REQUIRE(assignment.trailAt(0) == encodeLit(lit_true()));
		REQUIRE(assignment.trailAt(1) == encodeLit(ctx.stepLiteral()));
		REQUIRE(assignment.trailAt(2) == Potassco::lit(a1));
		REQUIRE(assignment.trailAt(3) == Potassco::neg(a2));
		REQUIRE(assignment.level() == 3);
		REQUIRE((assignment.trailBegin(0) == 0 && assignment.trailEnd(0) == 1));
		REQUIRE((assignment.trailBegin(1) == 1 && assignment.trailEnd(1) == 2));
		REQUIRE((assignment.trailBegin(2) == 2 && assignment.trailEnd(2) == 3));
		REQUIRE((assignment.trailBegin(3) == 3 && assignment.trailEnd(3) == 4));
		REQUIRE(assignment.level(a1) == 2);
		REQUIRE(assignment.level(a2) == 3);
	}

	SECTION("testClingoAssignmentContainsAllProblemVars") {
		ClingoAssignment assignment(*ctx.master());

		// Add vars to shared context but do not yet commit to solver
		Var v1 = ctx.addVar(Var_t::Atom);
		Var v2 = ctx.addVar(Var_t::Atom);
		CHECK(ctx.validVar(v1));
		CHECK(ctx.validVar(v1));
		CHECK_FALSE(ctx.master()->validVar(v1));
		CHECK_FALSE(ctx.master()->validVar(v2));
		CHECK(ctx.master()->numFreeVars() == 0);

		CHECK(assignment.size() == 3);
		CHECK(assignment.trailSize() == 1);
		CHECK(assignment.trailBegin(0) == 0);
		CHECK(assignment.trailAt(0) == encodeLit(lit_true()));
		CHECK(assignment.trailEnd(0) == 1);
		CHECK_FALSE(assignment.isTotal());
		CHECK(assignment.unassigned() == 2);
		CHECK(assignment.value(encodeLit(posLit(v1))) == Potassco::Value_t::Free);
		CHECK(assignment.value(encodeLit(posLit(v1))) == Potassco::Value_t::Free);

	}

	SECTION("testAssignment") {
		class Prop : public Potassco::AbstractPropagator {
		public:
			Prop() {}
			virtual void propagate(Potassco::AbstractSolver&, const ChangeList&)  {}
			virtual void undo(const Potassco::AbstractSolver&, const ChangeList&) {}
			virtual void check(Potassco::AbstractSolver& s) {
				const Potassco::AbstractAssignment& a = s.assignment();
				REQUIRE_FALSE(a.hasConflict());
				REQUIRE(a.level() == 2);
				REQUIRE(a.value(v1) == Potassco::Value_t::True);
				REQUIRE(a.value(v2) == Potassco::Value_t::False);
				REQUIRE(a.isTrue(v1));
				REQUIRE(a.isFalse(v2));
				REQUIRE(a.isTrue(Potassco::neg(v2)));
				REQUIRE(a.level(v1) == 1);
				REQUIRE(a.level(v2) == 2);
				REQUIRE_FALSE(a.hasLit(v2+1));
				REQUIRE(a.decision(0) == encodeLit(lit_true()));
				REQUIRE(a.decision(1) == v1);
				REQUIRE(a.decision(2) == Potassco::neg(v2));
				REQUIRE(a.trailSize() == 3);
				REQUIRE(a.trailAt(0) == encodeLit(lit_true()));
				REQUIRE(a.trailAt(1) == v1);
				REQUIRE(a.trailAt(2) == Potassco::neg(v2));
				REQUIRE(a.trailBegin(0) == 0);
				REQUIRE(a.trailEnd(0) == 1);
				REQUIRE(a.trailBegin(1) == 1);
				REQUIRE(a.trailEnd(1) == 2);
				REQUIRE(a.trailBegin(2) == 2);
				REQUIRE(a.trailEnd(2) == 3);
			}
			Potassco::Lit_t v1, v2;
		} prop;
		test.addVars(2);
		prop.v1 = encodeLit(posLit(v[1]));
		prop.v2 = encodeLit(posLit(v[2]));
		MyInit pp(prop);
		pp.applyConfig(*ctx.master());
		ctx.endInit();
		ctx.master()->assume(posLit(v[1])) && ctx.master()->propagate();
		ctx.master()->assume(negLit(v[2])) && ctx.master()->propagate();
		ctx.master()->search(0, 0);
	}

	SECTION("testPropagateChange") {
		test.addVars(5);
		tp.addWatch(posLit(v[1]));
		tp.addWatch(posLit(v[1])); // ignore duplicates
		tp.addWatch(posLit(v[2]));
		tp.addWatch(posLit(v[3]));
		tp.addWatch(negLit(v[3]));
		tp.addWatch(negLit(v[4]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		s.assume(posLit(v[1])) && s.propagate();
		REQUIRE((prop.change.size() == 1 && prop.change[0] == posLit(v[1])));

		s.assume(negLit(v[4])) && s.force(posLit(v[2]), 0) && s.propagate();
		REQUIRE((prop.change.size() == 2 && prop.change[0] == negLit(v[4]) && prop.change[1] == posLit(v[2])));
		prop.change.clear();
		s.undoUntil(s.decisionLevel()-1);
		REQUIRE((prop.change.size() == 2 && prop.change[0] == negLit(v[4]) && prop.change[1] == posLit(v[2])));
		s.undoUntil(s.decisionLevel()-1);
		REQUIRE((prop.change.size() == 1 && prop.change[0] == posLit(v[1])));
		prop.change.clear();
		s.assume(negLit(v[2])) && s.propagate();
		REQUIRE(prop.change.empty());
	}
	SECTION("testAddClause") {
		test.addVars(3);
		tp.addWatch(prop.fire = negLit(v[3]));
		prop.addToClause(posLit(v[1]));
		prop.addToClause(posLit(v[2]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		s.assume(negLit(v[3])) && s.propagate();
		REQUIRE(ctx.numLearntShort() == 1);
	}
	SECTION("testAddUnitClause") {
		test.addVars(3);
		tp.addWatch(prop.fire = negLit(v[3]));
		prop.addToClause(posLit(v[1]));
		prop.addToClause(posLit(v[2]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		s.assume(negLit(v[2])) && s.propagate();
		s.assume(negLit(v[3])) && s.propagate();
		REQUIRE(ctx.numLearntShort() == 1);
		REQUIRE(s.isTrue(posLit(v[1])));
		REQUIRE((prop.change.size() == 1 && prop.change[0] == negLit(v[3])));
	}
	SECTION("testAddUnitClauseWithUndo") {
		test.addVars(5);
		prop.fire = posLit(v[5]);
		tp.addWatch(posLit(v[3]));
		tp.addWatch(posLit(v[5]));
		prop.addToClause(posLit(v[1]));
		prop.addToClause(posLit(v[2]));
		prop.addToClause(posLit(v[3]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		s.assume(negLit(v[1])) && s.propagate();
		s.assume(posLit(v[4])) && s.propagate();
		s.assume(negLit(v[2])) && s.propagate();
		s.assume(posLit(v[5])) && s.propagate();
		REQUIRE(ctx.numLearntShort() == 1);
		REQUIRE(s.decisionLevel() == 3);
		s.undoUntil(2);
		REQUIRE(std::find(prop.change.begin(), prop.change.end(), posLit(v[3])) != prop.change.end());
	}
	SECTION("testAddUnsatClause") {
		test.addVars(3);
		tp.addWatch(prop.fire = negLit(v[3]));
		prop.addToClause(posLit(v[1]));
		prop.addToClause(posLit(v[2]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		s.assume(negLit(v[2])) && s.propagate();
		s.assume(negLit(v[1])) && s.propagate();
		s.assume(negLit(v[3]));
		s.pushRootLevel(2);
		REQUIRE_FALSE(s.propagate());
		INFO("do not add conflicting constraint");
		REQUIRE(ctx.numLearntShort() == 0);
		s.popRootLevel(1);
		REQUIRE(s.decisionLevel() == 1);
		prop.clause.clear();
		prop.addToClause(negLit(v[2]));
		prop.addToClause(posLit(v[3]));
		s.assume(negLit(v[3]));
		REQUIRE(s.propagate());
		INFO("do not add sat constraint");
		REQUIRE(ctx.numLearntShort() == 0);
	}
	SECTION("testAddEmptyClause") {
		test.addVars(1);
		tp.addWatch(prop.fire = negLit(v[1]));
		prop.addToClause(negLit(0));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		s.assume(negLit(v[1]));
		REQUIRE_FALSE(s.propagate());
	}
	SECTION("testAddSatClause") {
		test.addVars(3);
		tp.addWatch(prop.fire = negLit(v[3]));
		prop.addToClause(posLit(v[1]));
		prop.addToClause(posLit(v[2]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		s.assume(posLit(v[1])) && s.force(negLit(v[2]), posLit(v[1])) && s.propagate();
		s.assume(negLit(v[3]));
		REQUIRE((s.decisionLevel() == 2 && !s.hasConflict()));
		REQUIRE(s.propagate());
		REQUIRE(uint32(2) == s.decisionLevel());
	}
	SECTION("testAddClauseOnModel") {
		test.addVars(3);
		prop.addToClause(posLit(v[1]));
		prop.addToClause(posLit(v[3]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		ValueRep v = s.search(-1, -1);
		REQUIRE((v == value_true && s.numFreeVars() == 0));
		REQUIRE(ctx.shortImplications().numLearnt() == 1);
	}
	SECTION("testAddConflictOnModel") {
		test.addVars(3);
		prop.addToClause(negLit(v[1]));
		prop.addToClause(negLit(v[2]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();
		Solver& s = *ctx.master();
		s.assume(posLit(v[1]));
		s.force(posLit(v[2]), posLit(v[1]));
		s.propagate();
		s.assume(posLit(v[3])) && s.propagate();
		REQUIRE((!s.hasConflict() && s.numFreeVars() == 0));
		REQUIRE_FALSE(s.getPost(PostPropagator::priority_class_general)->isModel(s));
		REQUIRE(s.hasConflict());
		REQUIRE((s.decisionLevel() == 1 && s.resolveConflict()));
	}

	SECTION("testAddStatic") {
		test.addVars(2);
		prop.addToClause(posLit(v[1]));
		prop.addToClause(posLit(v[2]));
		prop.fire = lit_true();
		prop.clProp = Potassco::Clause_t::Static;
		tp.addWatch(negLit(v[1]));
		tp.applyConfig(*ctx.master());
		ctx.endInit();

		Solver& s = *ctx.master();
		REQUIRE(s.numWatches(negLit(v[2])) == 0);
		s.assume(negLit(v[1])) && s.propagate();
		REQUIRE(s.numWatches(negLit(v[2])) == 1);
		s.reduceLearnts(1.0);
		REQUIRE(s.numWatches(negLit(v[2])) == 1);
	}
	SECTION("with facade") {
		ClaspConfig config;
		ClaspFacade libclasp;
		config.addConfigurator(&tp);
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		lpAdd(asp, "{x1;x2}.");
		asp.endProgram();
		SECTION("testAttachToSolver") {
			for (Var v = 1; v <= libclasp.ctx.numVars(); ++v) {
				tp.addWatch(posLit(v));
				tp.addWatch(negLit(v));
			}
			REQUIRE(prop.change.empty());
			libclasp.prepare();
			libclasp.solve();
			REQUIRE_FALSE(prop.change.empty());
#if CLASP_HAS_THREADS
			config.solve.setSolvers(2);
			libclasp.update(true);
			libclasp.prepare();
			REQUIRE((libclasp.ctx.concurrency() == 2 && libclasp.ctx.hasSolver(1)));
			libclasp.solve();
			REQUIRE(libclasp.ctx.solver(1)->getPost(PostPropagator::priority_class_general) != 0);
			config.solve.setSolvers(1);
			libclasp.update(true);
			libclasp.prepare();
			REQUIRE(libclasp.ctx.concurrency() == 1);
			config.solve.setSolvers(2);
			libclasp.update(true);
			libclasp.solve();
			REQUIRE(libclasp.ctx.solver(1)->getPost(PostPropagator::priority_class_general) != 0);
#endif
		}
		SECTION("testAddVolatile") {
			tp.addWatch(negLit(1));
			prop.addToClause(posLit(1));
			prop.addToClause(posLit(2));
			libclasp.prepare();
			prop.fire = libclasp.ctx.stepLiteral();
			prop.clProp = Potassco::Clause_t::Volatile;
			libclasp.solve();
			REQUIRE(libclasp.ctx.numLearntShort() == 1);
			libclasp.update();
			REQUIRE(libclasp.ctx.numLearntShort() == 0);
		}
		SECTION("testAddVolatileStatic") {
			tp.addWatch(negLit(1));
			prop.addToClause(posLit(1));
			prop.addToClause(posLit(2));
			libclasp.prepare();
			prop.fire = libclasp.ctx.stepLiteral();
			prop.clProp = Potassco::Clause_t::VolatileStatic;
			libclasp.solve();
			REQUIRE(libclasp.ctx.master()->numWatches(negLit(2)) == 1);
			libclasp.update();
			REQUIRE(libclasp.ctx.master()->numWatches(negLit(2)) == 0);
		}
		SECTION("testLookaheadBug") {
			config.addSolver(0).lookType = Var_t::Atom;
			SatBuilder& sat = libclasp.startSat(config);
			sat.prepareProblem(2);
			LitVec clause;
			clause.push_back(negLit(1));
			clause.push_back(negLit(2));
			sat.addClause(clause);
			clause.pop_back();
			clause.push_back(posLit(2));
			sat.addClause(clause);
			tp.addWatch(negLit(1));
			libclasp.prepare();
			REQUIRE(libclasp.ctx.master()->isTrue(negLit(1)));
			REQUIRE(prop.change.size() == 1);
			REQUIRE(prop.change[0] == negLit(1));
		}
	}
	SECTION("with special propagator") {
		ClaspConfig config;
		ClaspFacade libclasp;
		SECTION("test push variables") {
			class AddVar : public Potassco::AbstractPropagator {
			public:
				typedef Potassco::Lit_t Lit_t;
				explicit AddVar(uint32 nAux) : aux_(nAux), next_(1) {}
				virtual void propagate(Potassco::AbstractSolver& s, const ChangeList&) {
					if (aux_) {
						const Potassco::AbstractAssignment& as = s.assignment();
						while (as.hasLit(next_)) { ++next_; }
						Lit_t x = s.addVariable();
						REQUIRE(x == next_);
						REQUIRE((!s.hasWatch(x) && !s.hasWatch(-x)));
						s.addWatch(x);
						REQUIRE((s.hasWatch(x) && !s.hasWatch(-x)));
						s.addWatch(-x);
						REQUIRE((s.hasWatch(x) && s.hasWatch(-x)));
						s.removeWatch(x);
						REQUIRE((!s.hasWatch(x) && s.hasWatch(-x)));
						s.removeWatch(-x);
						REQUIRE((!s.hasWatch(x) && !s.hasWatch(-x)));
						s.addWatch(x); s.addWatch(-x);
						--aux_;
					}
				}
				virtual void undo(const Potassco::AbstractSolver&, const ChangeList&) {}
				virtual void check(Potassco::AbstractSolver&) {}
				uint32 aux_;
				Lit_t  next_;
			} prop(2);
			MyInit pp(prop);
			config.addConfigurator(&pp);
			Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
			lpAdd(asp, "{x1;x2}.");
			asp.endProgram();
			pp.addWatch(posLit(1));
			pp.addWatch(negLit(1));
			pp.addWatch(posLit(2));
			pp.addWatch(negLit(2));
			SECTION("only during solving") {
				libclasp.prepare();
				uint32 nv = libclasp.ctx.numVars();
				uint32 sv = libclasp.ctx.master()->numVars();
				REQUIRE(nv == 3); // x1, x2 + step var
				REQUIRE(sv == 3);
				libclasp.solve();
				REQUIRE(nv == libclasp.ctx.numVars());
				REQUIRE(sv == libclasp.ctx.master()->numVars());
			}
			SECTION("also during init") {
				libclasp.ctx.addUnary(posLit(1));
				libclasp.prepare();
				uint32 nv = libclasp.ctx.numVars();
				uint32 sv = libclasp.ctx.master()->numVars();
				REQUIRE(nv == 3); // x1, x2 + step var
				REQUIRE(sv == 4);
				REQUIRE(libclasp.ctx.stepLiteral().var() == 3);
				libclasp.solve();
				REQUIRE(nv == libclasp.ctx.numVars());
				REQUIRE(nv == libclasp.ctx.master()->numVars());
			}
		}
		SECTION("testAuxVarMakesClauseVolatile") {
			class AddAuxClause : public Potassco::AbstractPropagator {
			public:
				typedef Potassco::Lit_t Lit_t;
				explicit AddAuxClause() { aux = 0;  nextStep = false; }
				virtual void propagate(Potassco::AbstractSolver& s, const ChangeList&) {
					if (!aux) {
						aux = s.addVariable();
						Potassco::LitVec clause;
						for (Lit_t i = 1; i < aux; ++i) {
							if (s.hasWatch(i)) {
								clause.push_back(-i);
							}
						}
						clause.push_back(-aux);
						s.addClause(Potassco::toSpan(clause), Potassco::Clause_t::Static);
					}
					REQUIRE((!nextStep || !s.assignment().hasLit(aux)));
				}
				virtual void undo(const Potassco::AbstractSolver&, const ChangeList&) {}
				virtual void check(Potassco::AbstractSolver&) {}
				Lit_t aux;
				bool  nextStep;
			} prop;
			MyInit pp(prop);
			config.addConfigurator(&pp);
			Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
			lpAdd(asp, "{x1;x2}.");
			asp.endProgram();
			pp.addWatch(posLit(1));
			pp.addWatch(posLit(2));
			LitVec assume;
			libclasp.prepare();
			assume.push_back(posLit(1));
			assume.push_back(posLit(2));
			libclasp.solve(assume);
			libclasp.update();
			prop.nextStep = true;
			libclasp.solve(assume);
		}

		SECTION("testRootLevelBug") {
			class Prop : public Potassco::AbstractPropagator {
			public:
				Prop() {}
				virtual void propagate(Potassco::AbstractSolver& s, const ChangeList&) {
					REQUIRE(s.assignment().level() != 0);
					for (Potassco::Atom_t a = 2; a != 4; ++a) {
						Potassco::Lit_t pos = a;
						Potassco::Lit_t neg = -pos;
						if (!s.addClause(Potassco::toSpan(&pos, 1))) { return; }
						if (!s.addClause(Potassco::toSpan(&neg, 1))) { return; }
					}
				}
				virtual void undo(const Potassco::AbstractSolver&, const ChangeList&) {}
				virtual void check(Potassco::AbstractSolver&) {}
			} prop;
			MyInit pp(prop);
			config.addConfigurator(&pp);
			Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
			lpAdd(asp, "{x1;x2}.");
			asp.endProgram();
			pp.addWatch(posLit(1));
			pp.addWatch(negLit(1));
			pp.addWatch(posLit(2));
			pp.addWatch(negLit(2));
			libclasp.prepare();
			REQUIRE(libclasp.solve().unsat());
		}

		SECTION("testRelocationBug") {
			class Prop : public Potassco::AbstractPropagator {
			public:
				Prop() {}
				virtual void propagate(Potassco::AbstractSolver& s, const ChangeList& changes) {
					Potassco::LitVec cmp(begin(changes), end(changes));
					Potassco::LitVec clause;
					clause.assign(1, 0);
					for (uint32 i = 1; i <= s.assignment().level(); ++i) {
						clause.push_back(-s.assignment().decision(i));
					}
					for (Potassco::Lit_t lit = 1; s.assignment().hasLit(lit); ++lit) {
						if (s.assignment().value(lit) == Potassco::Value_t::Free) {
							clause[0] = lit;
							s.addClause(Potassco::toSpan(clause));
							s.propagate();
						}
					}
					REQUIRE(std::memcmp(&cmp[0], changes.first, changes.size * sizeof(Potassco::Lit_t)) == 0);
				}
				virtual void undo(const Potassco::AbstractSolver&, const ChangeList&) {}
				virtual void check(Potassco::AbstractSolver&) {}
			} prop;
			MyInit pp(prop);
			config.addConfigurator(&pp);
			Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
			lpAdd(asp, "{x1;x2;x3;x4;x5;x6;x7;x8;x9;x10;x11;x12;x13;x14;x15;x16}.");
			asp.endProgram();
			for (Var v = 1; v <= libclasp.ctx.numVars(); ++v) {
				pp.addWatch(posLit(v));
				pp.addWatch(negLit(v));
			}
			libclasp.prepare();
			REQUIRE(libclasp.solve().sat());
		}
	}
	SECTION("test check mode") {
		ClaspConfig config;
		ClaspFacade libclasp;
		class Prop : public Potassco::AbstractPropagator {
		public:
			Prop() : last(0), checks(0), props(0), totals(0), undos(0), fire(false) {}
			virtual void propagate(Potassco::AbstractSolver& s, const ChangeList& c) {
				const Potassco::AbstractAssignment& a = s.assignment();
				++props;
				if (*Potassco::begin(c) == last) { return; }
				for (int x = *Potassco::begin(c) + 1; a.hasLit(x); ++x) {
					if (a.value(x) == Potassco::Value_t::Free) {
						last = x;
						s.addClause(Potassco::toSpan(&x, 1));
						break;
					}
				}
			}
			virtual void undo(const Potassco::AbstractSolver&, const ChangeList&) { ++undos; }
			virtual void check(Potassco::AbstractSolver& s) {
				const Potassco::AbstractAssignment& a = s.assignment();
				++checks;
				totals += a.isTotal();
				if (fire) {
					for (int x = 1; a.hasLit(x); ++x) {
						if (a.value(x) == Potassco::Value_t::Free) {
							s.addClause(Potassco::toSpan(&x, 1));
							return;
						}
					}
					REQUIRE(a.isTotal());
					REQUIRE(a.level() == 0);
				}
			}
			int last;
			int checks;
			int props;
			int totals;
			int undos;
			bool fire;
		} prop;
		MyInit pp(prop);
		config.addConfigurator(&pp);
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp, "{x1;x2;x3;x4;x5}.");
		asp.endProgram();
		SECTION("test check and propagate") {
			prop.fire = true;
			pp.addWatch(posLit(1));
			pp.addWatch(posLit(2));
			pp.addWatch(posLit(3));
			pp.addWatch(posLit(4));
			pp.addWatch(posLit(5));
			pp.enableClingoPropagatorCheck(ClingoPropagatorCheck_t::Fixpoint);
			libclasp.prepare();
			REQUIRE(libclasp.ctx.master()->numFreeVars() == 0);
		}
		SECTION("test check is called only once per fixpoint") {
			int expectedUndos = 0;
			pp.enableClingoPropagatorCheck(ClingoPropagatorCheck_t::Fixpoint);
			SECTION("fixpoint default undo") {
				expectedUndos = 0;
			}
			SECTION("fixpoint always undo") {
				pp.enableClingoPropagatorUndo(ClingoPropagatorUndo_t::Always);
				expectedUndos = 1;
			}
			libclasp.prepare();
			REQUIRE(prop.checks == 1u);
			libclasp.ctx.master()->propagate();
			REQUIRE(prop.checks == 1u);
			libclasp.ctx.master()->pushRoot(posLit(1));
			REQUIRE(prop.checks == 2u);
			libclasp.ctx.master()->assume(posLit(2)) && libclasp.ctx.master()->propagate();
			REQUIRE(prop.checks == 3u);
			libclasp.ctx.master()->propagate();
			REQUIRE(prop.checks == 3u);
			libclasp.ctx.master()->restart();
			REQUIRE(prop.undos == expectedUndos);
			libclasp.ctx.master()->propagate();
			INFO("Restart introduces new fix point");
			REQUIRE(prop.checks == 4u);
		}
		SECTION("with mode total check is called once on total") {
			int expectedUndos = 0;
			pp.enableClingoPropagatorCheck(ClingoPropagatorCheck_t::Total);
			SECTION("total default undo") {
				expectedUndos = 0;
			}
			SECTION("total always undo") {
				pp.enableClingoPropagatorUndo(ClingoPropagatorUndo_t::Always);
				expectedUndos = 1;
			}
			libclasp.solve();
			libclasp.ctx.master()->undoUntil(0);
			REQUIRE(prop.checks == 1u);
			REQUIRE(prop.totals == 1u);
			REQUIRE(prop.undos == expectedUndos);
		}
		SECTION("with mode fixpoint check is called once on total") {
			pp.enableClingoPropagatorCheck(ClingoPropagatorCheck_t::Fixpoint);
			libclasp.solve();
			REQUIRE(prop.checks > 1u);
			REQUIRE(prop.totals == 1u);
		}
	}
}

TEST_CASE("Clingo propagator init", "[facade][propagator]") {
	typedef ClingoPropagatorInit MyInit;

	PropagatorTest test;
	SharedContext& ctx = test.ctx;
	MyProp         prop;
	DebugLock      debugLock;
	MyInit         init(prop, &debugLock);

	test.addVars(5);
	init.prepare(ctx);
	Solver& s0 = *ctx.master();
	SECTION("add watches") {
		init.addWatch(posLit(1));
		init.addWatch(posLit(2));
		init.addWatch(posLit(4));
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(s0.hasWatch(posLit(1), pp));
		REQUIRE(s0.hasWatch(posLit(2), pp));
		REQUIRE(s0.hasWatch(posLit(4), pp));
		REQUIRE_FALSE(s0.hasWatch(posLit(3), pp));

		REQUIRE(test.isFrozen(posLit(1)));
		REQUIRE(test.isFrozen(posLit(2)));
		REQUIRE(test.isFrozen(posLit(4)));
		REQUIRE_FALSE(test.isFrozen(posLit(3)));
	}
	SECTION("freezeLit") {
		init.addWatch(posLit(1));
		init.removeWatch(posLit(1));
		init.freezeLit(posLit(1));
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
		REQUIRE(test.isFrozen(posLit(1)));
	}
	SECTION("init acquires all problem vars") {
		Var v = ctx.addVar(Var_t::Atom);
		init.addWatch(posLit(v));
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(s0.hasWatch(posLit(v), pp));
	}
	SECTION("ignore duplicate watches from init") {
		init.addWatch(posLit(1));
		init.addWatch(posLit(1));
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(s0.hasWatch(posLit(1), pp));
		s0.removeWatch(posLit(1), pp);
		REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
	}
	SECTION("ignore duplicates on solver-specific init") {
		init.addWatch(posLit(1));
		init.addWatch(0, posLit(1));
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(s0.hasWatch(posLit(1), pp));
		s0.removeWatch(posLit(1), pp);
		REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
	}
	SECTION("add solver-specific watches") {
		Solver& s1 = ctx.pushSolver();
		init.prepare(ctx);
		init.addWatch(posLit(1));     // add to both
		init.addWatch(0, posLit(2));
		init.addWatch(1, posLit(3));
		init.applyConfig(s0);
		init.applyConfig(s1);
		ctx.endInit(true);
		PostPropagator* pp0 = s0.getPost(PostPropagator::priority_class_general);
		PostPropagator* pp1 = s1.getPost(PostPropagator::priority_class_general);
		REQUIRE(s0.hasWatch(posLit(1), pp0));
		REQUIRE(s1.hasWatch(posLit(1), pp1));

		REQUIRE(s0.hasWatch(posLit(2), pp0));
		REQUIRE_FALSE(s1.hasWatch(posLit(2), pp1));

		REQUIRE_FALSE(s0.hasWatch(posLit(3), pp0));
		REQUIRE(s1.hasWatch(posLit(3), pp1));

		REQUIRE(test.isFrozen(posLit(1)));
		REQUIRE(test.isFrozen(posLit(2)));
		REQUIRE(test.isFrozen(posLit(3)));
	}
	SECTION("don't add removed watch") {
		Solver& s1 = ctx.pushSolver();
		init.prepare(ctx);
		// S0: [1,2,3]
		// S1: [1, ,3]
		init.addWatch(posLit(1));
		init.addWatch(posLit(2));
		init.addWatch(posLit(3));
		init.removeWatch(1, posLit(2));
		init.applyConfig(s0);
		init.applyConfig(s1);
		ctx.endInit(true);

		PostPropagator* pp0 = s0.getPost(PostPropagator::priority_class_general);
		PostPropagator* pp1 = s1.getPost(PostPropagator::priority_class_general);
		REQUIRE(s0.hasWatch(posLit(1), pp0));
		REQUIRE(s0.hasWatch(posLit(2), pp0));
		REQUIRE(s0.hasWatch(posLit(3), pp0));

		REQUIRE(s1.hasWatch(posLit(1), pp1));
		REQUIRE_FALSE(s1.hasWatch(posLit(2), pp0));
		REQUIRE(s1.hasWatch(posLit(3), pp1));

		REQUIRE(test.isFrozen(posLit(1)));
		REQUIRE(test.isFrozen(posLit(2)));
		REQUIRE(test.isFrozen(posLit(3)));
	}

	SECTION("last call wins") {
		init.addWatch(posLit(1));
		init.removeWatch(0, posLit(1));
		init.addWatch(posLit(1));
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(s0.hasWatch(posLit(1), pp));
		REQUIRE(test.isFrozen(posLit(1)));
	}

	SECTION("watched facts are propagated") {
		init.addWatch(posLit(1));
		ctx.startAddConstraints();
		ctx.addUnary(posLit(1));
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(prop.change.size() == 1);
		REQUIRE(prop.change[0] == posLit(1));
		REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
	}
	SECTION("facts can be watched even after propagate") {
		init.addWatch(posLit(1));
		ctx.startAddConstraints();
		ctx.addUnary(posLit(1));
		s0.propagate();
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(prop.change.size() == 1);
		REQUIRE(prop.change[0] == posLit(1));
		REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
		REQUIRE(test.isFrozen(posLit(1)));
	}
	SECTION("facts are propagated only once") {
		init.addWatch(posLit(1));
		ctx.startAddConstraints();
		ctx.addUnary(posLit(1));
		init.applyConfig(s0);
		ctx.endInit();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(prop.change.size() == 1);
		REQUIRE(prop.change[0] == posLit(1));
		prop.change.clear();
		ctx.unfreeze();
		init.unfreeze(ctx);
		ctx.startAddConstraints();
		ctx.addUnary(posLit(2));
		init.addWatch(posLit(1));
		init.addWatch(posLit(2));
		ctx.endInit();
		REQUIRE(prop.change.size() == 1);
		REQUIRE(prop.change[0] == posLit(2));
	}

	SECTION("init optionally keeps history so that future solvers get correct watches") {
		init.enableHistory(true);
		Solver& s1 = ctx.pushSolver();
		init.prepare(ctx);
		// S0: [1,2,3]
		// S1: [1, ,3]
		// S2: [ ,2, ,4]
		init.addWatch(posLit(1));
		init.addWatch(posLit(2));
		init.addWatch(posLit(3));
		init.removeWatch(1, posLit(2));
		init.removeWatch(2, posLit(1));
		init.removeWatch(2, posLit(3));
		init.addWatch(2, posLit(4));
		init.applyConfig(s0);
		init.applyConfig(s1);
		// don't add s2 yet
		ctx.endInit(true);

		ctx.unfreeze();
		init.unfreeze(ctx);
		Solver& s2 = ctx.pushSolver();
		init.prepare(ctx);
		ctx.startAddConstraints();
		init.addWatch(posLit(5));
		init.applyConfig(s2);
		ctx.endInit(true);
		PostPropagator* pp2 = s2.getPost(PostPropagator::priority_class_general);

		REQUIRE_FALSE(s2.hasWatch(posLit(1), pp2));
		REQUIRE(s2.hasWatch(posLit(2), pp2));
		REQUIRE_FALSE(s2.hasWatch(posLit(3), pp2));
		REQUIRE(s2.hasWatch(posLit(4), pp2));
		REQUIRE(s2.hasWatch(posLit(5), pp2));
	}

	SECTION("test init-solve interplay") {
		class Prop : public Potassco::AbstractPropagator {
		public:
			Prop() {}
			virtual void propagate(Potassco::AbstractSolver&, const ChangeList&) {}
			virtual void undo(const Potassco::AbstractSolver&, const ChangeList&)  {}
			virtual void check(Potassco::AbstractSolver& s) {
				while (!add.empty()) {
					s.addWatch(encodeLit(add.back()));
					add.pop_back();
				}
				while (!remove.empty()) {
					s.removeWatch(encodeLit(remove.back()));
					remove.pop_back();
				}
			}
			void addWatch(Literal lit)    { add.push_back(lit); }
			void removeWatch(Literal lit) { remove.push_back(lit); }
			LitVec add;
			LitVec remove;
		} prop;
		MyInit init(prop);
		init.enableClingoPropagatorCheck(ClingoPropagatorCheck_t::Fixpoint);

		SECTION("ignore watches already added in init") {
			init.addWatch(posLit(1));
			prop.addWatch(posLit(1));
			init.applyConfig(s0);
			ctx.endInit();
			PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
			REQUIRE(s0.hasWatch(posLit(1), pp));
			REQUIRE(prop.add.empty());
			s0.removeWatch(posLit(1), pp);
			REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
		}

		SECTION("ignore watches in init already added during solving") {
			prop.addWatch(posLit(1));
			init.applyConfig(s0);
			ctx.endInit();
			PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
			REQUIRE(prop.add.empty());
			ctx.unfreeze();
			init.unfreeze(ctx);
			ctx.startAddConstraints();
			init.addWatch(posLit(1));
			init.addWatch(posLit(2));
			ctx.endInit();
			REQUIRE(s0.hasWatch(posLit(1), pp));
			REQUIRE(s0.hasWatch(posLit(2), pp));
			s0.removeWatch(posLit(1), pp);
			REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
		}

		SECTION("remove watch during solving") {
			init.addWatch(posLit(1));
			prop.removeWatch(posLit(1));
			init.applyConfig(s0);
			ctx.endInit();
			PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
			ctx.unfreeze();
			init.unfreeze(ctx);
			ctx.startAddConstraints();
			ctx.endInit();
			REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
		}

		SECTION("remove watch during solving then add on init") {
			init.addWatch(posLit(1));
			prop.removeWatch(posLit(1));
			init.applyConfig(s0);
			ctx.endInit();
			PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
			ctx.unfreeze();
			init.unfreeze(ctx);
			REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
			init.addWatch(posLit(1));
			ctx.startAddConstraints();
			ctx.endInit();
			REQUIRE(s0.hasWatch(posLit(1), pp));
		}

		SECTION("add watch during solving then remove on init") {
			prop.addWatch(posLit(1));
			init.applyConfig(s0);
			ctx.endInit();
			PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
			ctx.unfreeze();
			init.unfreeze(ctx);
			REQUIRE(s0.hasWatch(posLit(1), pp));
			init.removeWatch(posLit(1));
			ctx.startAddConstraints();
			ctx.endInit();
			REQUIRE_FALSE(s0.hasWatch(posLit(1), pp));
		}
	}
}
TEST_CASE("Clingo propagator init with facade", "[facade][propagator]") {
	typedef ClingoPropagatorInit MyInit;

	ClaspConfig config;
	ClaspFacade libclasp;
	SharedContext& ctx = libclasp.ctx;
	MyProp         prop1, prop2;
	DebugLock      debugLock;
	MyInit         init1(prop1, &debugLock), init2(prop2, &debugLock);

	SECTION("init acquires all problem vars") {
		config.addConfigurator(&init1);
		config.addConfigurator(&init2);
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp, "{x1}.");
		asp.endProgram();
		Var v = ctx.addVar(Var_t::Atom);
		init1.addWatch(posLit(v));
		init2.addWatch(negLit(v));
		ctx.endInit();
		Solver& s0 = *ctx.master();
		PostPropagator* pp = s0.getPost(PostPropagator::priority_class_general);
		REQUIRE(s0.hasWatch(posLit(v), pp));
		REQUIRE(pp->next != 0);
		REQUIRE(s0.hasWatch(negLit(v), pp->next));
	}
}

TEST_CASE("Clingo heuristic", "[facade][heuristic]") {
	class ClingoHeu : public Potassco::AbstractHeuristic {
	public:
		ClingoHeu() : lock(0) {}
		virtual Lit decide(Potassco::Id_t, const Potassco::AbstractAssignment& assignment, Lit fallback) {
			REQUIRE((!lock || lock->locked));
			REQUIRE_FALSE(assignment.isTotal());
			REQUIRE(assignment.value(fallback) == Potassco::Value_t::Free);
			fallbacks.push_back(fallback);
			for (Potassco::Atom_t i = Potassco::atom(fallback) + 1;; ++i) {
				if (!assignment.hasLit(i))
					i = 1;
				if (assignment.value(i) == Potassco::Value_t::Free) {
					selected.push_back(Potassco::neg(i));
					return selected.back();
				}
			}
		}
		Potassco::LitVec selected;
		Potassco::LitVec fallbacks;
		DebugLock*       lock;
	};
	ClaspConfig config;
	ClaspFacade libclasp;
	ClingoHeu   heuristic;
	SECTION("Factory") {
		config.setHeuristicCreator(new ClingoHeuristic::Factory(heuristic));
		DecisionHeuristic* heu = config.heuristic(0);
		REQUIRE(dynamic_cast<ClingoHeuristic*>(heu) != 0);
		REQUIRE(dynamic_cast<ClaspBerkmin*>(dynamic_cast<ClingoHeuristic*>(heu)->fallback()) != 0);
		delete heu;
	}

	SECTION("Clingo heuristic is called with fallback") {
		SolverParams& opts = config.addSolver(0);
		opts.heuId    = Heuristic_t::None;
		config.setHeuristicCreator(new ClingoHeuristic::Factory(heuristic));
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp, "{x1;x2;x3}.");
		asp.endProgram();
		libclasp.prepare();

		SingleOwnerPtr<DecisionHeuristic> fallback(Heuristic_t::create(Heuristic_t::None, HeuParams()));
		Solver& s = *libclasp.ctx.master();

		while (s.numFreeVars() != 0) {
			Literal fb = fallback->doSelect(s);
			Literal lit = s.heuristic()->doSelect(s);
			REQUIRE(lit == decodeLit(heuristic.selected.back()));
			REQUIRE(fb  == decodeLit(heuristic.fallbacks.back()));
			s.assume(lit) && s.propagate();
		}

		REQUIRE(heuristic.selected.size() == s.numVars());
	}

	SECTION("Restricted lookahead decorates clingo heuristic") {
		SolverParams& opts = config.addSolver(0);
		opts.lookOps  = 2;
		opts.lookType = 1;
		opts.heuId    = Heuristic_t::Vsids;
		config.setHeuristicCreator(new ClingoHeuristic::Factory(heuristic));
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp, "{x1;x2;x3}.");
		asp.endProgram();
		libclasp.prepare();
		DecisionHeuristic* heu = libclasp.ctx.master()->heuristic();
		// heuristic is Restricted(Clingo(Vsids))
		REQUIRE(dynamic_cast<ClingoHeuristic*>(heu) == 0);
		REQUIRE(dynamic_cast<UnitHeuristic*>(heu) != 0);

		// Restricted does not forward to its decorated heuristic
		Literal lit = heu->doSelect(*libclasp.ctx.master());
		REQUIRE(heuristic.fallbacks.empty());
		REQUIRE(heuristic.selected.empty());
		libclasp.ctx.master()->assume(lit);

		// Last lookahead operation - disables restricted heuristic
		libclasp.ctx.master()->propagate();

		// Restricted is no longer enabled and should remove itself on this call
		lit = heu->doSelect(*libclasp.ctx.master());
		REQUIRE(heuristic.fallbacks.size() == 1);
		REQUIRE(heuristic.selected.size() == 1);
		REQUIRE(heuristic.selected.back() == encodeLit(lit));
		REQUIRE(heuristic.fallbacks.back() != heuristic.selected.size());

		// From now on, we only have Clingo(Vsids)
		heu = libclasp.ctx.master()->heuristic();
		REQUIRE(dynamic_cast<ClingoHeuristic*>(heu) != 0);
		REQUIRE(dynamic_cast<ClaspVsids*>(dynamic_cast<ClingoHeuristic*>(heu)->fallback()) != 0);
	}

	SECTION("Heuristic is called under lock") {
		DebugLock lock;
		heuristic.lock = &lock;

		config.setHeuristicCreator(new ClingoHeuristic::Factory(heuristic, &lock));
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
		lpAdd(asp, "{x1;x2;x3}.");
		asp.endProgram();
		libclasp.prepare();

		Solver& s = *libclasp.ctx.master();
		s.decideNextBranch();
		REQUIRE_FALSE(heuristic.selected.empty());
		REQUIRE_FALSE(lock.locked);
	}
}


} }
