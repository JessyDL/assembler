from argparse import ArgumentParser
from paradigm.build import *
import subprocess

class Assembler(object):
    def initialize(self, paradigm):
        parser = paradigm.initialize(os.path.dirname(os.path.realpath(__file__)))
        #parser.add_argument("--assimp", default="", nargs=None, help="path to an installed assimp library", dest="assimp_path")
        return parser
        
    def build(self, paradigm, args):
        p.parse(p.initialize())
        
    def __call__(self):
        p = Paradigm()
        parser = self.initialize(p)
        args, remaining_argv = p.parse(parser)
        
        generate_cmd = p.generate_command(args)
        #generate_cmd = generate_cmd + [ "-DPE_ASSIMP_PATH="+ args.assimp_path]
        build_cmd=""
        if(args.verbose):
            build_cmd = p.build_command(args)
        
        p.build(args.project_dir, generate_cmd, build_cmd)
        
def main():
    assembler = Assembler()
    assembler()

if __name__ == "__main__":main()