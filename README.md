# xk7c420t_fastqz_fxa_dual_channel
This is software of FASTQZ FPGA accelerator. This software can only be used in Linux OS.

# Usage
./fastqz command input output [reference]  


Commands:  
  c[Q] - compress input to output.fx?.zpaq (? = {h,b,q})  
  d    - decompress input.fx?.zpaq to output\n"  
  e[Q] - encode (fast) input to output.fx? (? = {h,b,q})  
  f    - fast decode input.fx? to output\n"  
  Use Q to quantize quality values to steps of size Q for better but lossy compression. Default is c1 or e1 (lossless).
