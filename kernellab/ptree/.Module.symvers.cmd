cmd_/home/jaeyeon/kernellab-handout/ptree/Module.symvers := sed 's/ko$$/o/' /home/jaeyeon/kernellab-handout/ptree/modules.order | scripts/mod/modpost -m -a   -o /home/jaeyeon/kernellab-handout/ptree/Module.symvers -e -i Module.symvers   -T -