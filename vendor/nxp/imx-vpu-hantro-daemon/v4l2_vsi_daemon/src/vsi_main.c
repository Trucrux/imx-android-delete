/****************************************************************************
*
*    Copyright 2019 - 2020 VeriSilicon Inc. All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

/**
 * @file vsi_main.c
 * @brief V4L2 Daemon main file.
 * @version 0.10- Initial version
 */
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "daemon_instance.h"
#include "hash_table.h"

typedef struct zombie_inst{
  v4l2_daemon_inst *pinst;
  struct timespec starttime;
  struct zombie_inst *next;
} zombie_inst;

static int64_t zombie_delaytime = 1000000000;
FILE *vsidaemonstdlog = NULL;
static HashTable ht;
static pthread_t maintid;
static jmp_buf mainsigbuf;
static zombie_inst *hzinst = NULL;
/**
 * @brief Worker thread of the encoder/decoder, it parse the command that
 received from V4L2 driver and
          process the state of the encoder/decoder. When encoder/decoder errors
 happens or ends without errors, thread will exit.
          There may be several threads for muti-instance.
 * @param[in] arg The thread parameters that passed by pthread_create.
 * @return NULL point if the thread exits.
 */
void *worker_threads(void *arg);

/**
 * @brief Initial the instance_id and other member, the semphore, the
 * bufferlist, the fifo instance and create the worker thread.
 * @param[in] h point    The daemon instance that in the HashTable.
 * @param[in] v4l2_msg point       The msg that received from the V4L2 driver.
 * @return 0 means instance initial successfully, otherwise unsuccessfully.
 */
static int32_t inst_init(v4l2_daemon_inst *h, vsi_v4l2_msg *v4l2_msg);

/**
 * @brief Destory worker instance, set the members of the instance to invalid
 * value, destroy the semphore, bufferlist, etc.
 * @param[in] h    The daemon instance that in the HashTable.
 * @return 0 means destroy successfully, otherwise unsuccessfully.
 */
int32_t inst_destroy(v4l2_daemon_inst *h);
static void destroy_heap(struct object_heap *heap,
                         void (*func)(struct object_heap *heap,
                                      struct object_base *object));

static void check_instance(v4l2_daemon_inst *cur_inst_p) {
  if (cur_inst_p == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR,
               "check_instance: got NULL instance, it should be malloced.\n");
    ASSERT(0);
  } else if (cur_inst_p->instance_id == 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR,
               "check_instance: got 0 instance_id, it should not happend.\n");
    ASSERT(0);
  }
}

static v4l2_daemon_inst *find_zombieinst(pthread_t tid)
{
    zombie_inst *pz = hzinst;

    while (pz) {
        if (pz->pinst->tid == tid)
            return pz->pinst;
        pz = pz->next;
    }
    return NULL;	
}

static void signalhandler(int signo) {
  v4l2_daemon_inst *inst_p;
  pthread_t tid = pthread_self();
  HANTRO_LOG(HANTRO_LEVEL_ERROR, "%s : sig: %d(%s), thread: %lX \r\n",
             __FUNCTION__, signo, strsignal(signo), pthread_self());

  inst_p = hash_table_find_bytid(&ht, tid);
  if (inst_p == NULL)
    inst_p = find_zombieinst(tid);
  if (inst_p != NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "%s: catch exception for ctx %lx !!!!!\r\n",
               __FUNCTION__, inst_p->instance_id);
    if (inst_p->flag & (1 << INST_CATCH_EXCEPTION)) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "%s: repeatedly catch exception for ctx %lx !!!!!\r",
        __FUNCTION__, inst_p->instance_id);
      inst_p->flag |= (1 << INST_FATAL_ERROR);
    } else {
      if (signo != SIGUSR1)
          inst_p->flag |= (1 << INST_CATCH_EXCEPTION);
      siglongjmp(inst_p->sigbuf, 1);
    }
  } else if (tid == maintid) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "%s: catch exception for main!!!!!\r\n",
               __FUNCTION__);
    siglongjmp(mainsigbuf, 1);
  } else {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "%s: unexpected thread id !!!!!\r\n",
               __FUNCTION__);
    siglongjmp(mainsigbuf, 1);
  }

  return;
}

static int register_sighandler(void) {
  int i;
  static const int siglist[] = {
      SIGHUP, SIGINT,  SIGILL,  SIGUSR1, SIGABRT, SIGBUS,
      SIGFPE, SIGSEGV, SIGTERM, SIGTSTP, SIGTTIN,
  };
  for (i = 0; i < sizeof(siglist) / sizeof(siglist[0]); i++) {
    if (signal(siglist[i], signalhandler) == SIG_ERR) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR, "fail to register signal %d.\n",
                 siglist[i]);
      return -1;
    }
  }
  return 0;
}

static int get_codec_info(struct vsi_v4l2_dev_info *hwinfo) {
#ifdef HAS_VSI_ENC
  if (vsi_enc_get_codec_format(hwinfo) == -1) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "No valid encoder.\n");
    hwinfo->encformat = 0;
  }
#endif
  if (vsi_dec_get_codec_format(hwinfo) == -1) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "No valid decoder.\n");
    hwinfo->decformat = 0;
  }

  return 0;
}

static int add_zombieinst(v4l2_daemon_inst *pinst, struct timespec *pstarttime)
{
    zombie_inst *pz = malloc(sizeof(zombie_inst));

    if (!pz)
        return -1;
    pz->next = NULL;
    pz->pinst = pinst;
    pz->pinst->flag |= (1 << INST_ZOMBIE);
    pz->starttime = *pstarttime;    
    if (hzinst == NULL)
        hzinst = pz;
    else {
        pz->next = hzinst->next;
        hzinst->next = pz;
    }
    return 0;
}

static int zombie_timeout(struct timespec *before)
{
    struct timespec after;
    int64_t elapse;

    clock_gettime(CLOCK_REALTIME, &after);
    elapse = ((int64_t)after.tv_sec - (int64_t)before->tv_sec) * (int64_t)1000000000
         + ((int64_t)after.tv_nsec - (int64_t)before->tv_nsec);
    return (elapse >= zombie_delaytime); //over 1 sec
}

static void check_zombieinst(void)
{
    zombie_inst *pz = hzinst, *pn, *prev = NULL;
    int ret = 0, done;

    while (pz) {
        done = 0;
        pn = pz->next;
        if (pz->pinst->flag & (1 << INST_DEAD))
            done = 1;
        else {
            ret = sem_trywait(&pz->pinst->sem_done);
            if (ret == 0)
                done = 1;
            else if (zombie_timeout(&pz->starttime))
                pthread_kill(pz->pinst->tid, SIGUSR1);
        }
        if (done) {
            HANTRO_LOG(HANTRO_LEVEL_WARNING, "release zombie inst %lx with %d\n", pz->pinst->instance_id, ret);
            if (done) {
                inst_destroy(pz->pinst);
                free(pz->pinst);
                free(pz);
            }
            if (prev == NULL)
                hzinst = pz = pn;
            else
                prev->next = pz = pn;
        } else {
            prev = pz;
            pz = pn;
        }
    }
}

static void removeall_zombieinst(void)
{
    int i;
    zombie_inst *pz = hzinst, *pn, *prev;
    struct timespec interval, rem;

    if (hzinst == NULL)
        return;
    while (pz) {
        pz->pinst->flag |= (1 << INST_FORCEEXIT);
        pthread_kill(pz->pinst->tid, SIGUSR1);
        pz = pz->next;
    }
    interval.tv_sec = 0;
    interval.tv_nsec = 100;
    for (i = 0; i < 100; i++) {
        nanosleep(&interval, &rem);
        pz = hzinst;
        prev = NULL;
        while (pz) {
            pn = pz->next;
            if (pz->pinst->flag & (1 << INST_DEAD)){
                HANTRO_LOG(HANTRO_LEVEL_WARNING, "release zombie inst %lx in tail\n", pz->pinst->instance_id);
                inst_destroy(pz->pinst);
                free(pz->pinst);
                free(pz);
                if (prev == NULL)
                    hzinst = pz = pn;
                else
                    prev->next = pz = pn;
            } else {
                prev = pz;
                pz = pn;
            }
        }
        if (hzinst == NULL)
            return;
    }
}


/**
 * @brief Main thread.
 * @return an int value.
 * @note The log file will be generated if macro LOG_OUTPUT is defined, by
 default this is not be defined.
         One device named /dev/v4l2 is used for communication between daemon and
 V4L2 driver, another device named /dev/mem is used for mmap,
         before workers begin to work, these should be opened. The global
 variables named pipe_fd and mmap_fd are the handles, respectively.
         In order to support mult-instance and increase efficiency, a hash table
 is introduced. The structure of the daemon instance is
         a member of the hash table structure HashTable. Note that the maximum
 instances is defined by MAX_STREAMS, which is 10, and this can
         be increased or decreased. If a valid instance id(not 0) is received,
 it will look up the instance id in the hash table, if found,
         this is not a new instance id, it has been created already, if not
 found, this may be a new instance id, it will insert this new instance
         id to the hash table and initial this new instance, a new worker is
 created. Note that the valid instance id value is from 1 to MAX_STREAMS.
         After a valid instance id is received, it will get the pointer of the
 daemon instance. Then send the command to the worker. If the command
         it that received from V4L2 is stream off or command stop, the main
 thread will wait for the semphore that the worker post and destroy the
         daemon instance. Note that the main thead of daemon never returns.
 */
int32_t main(int32_t argc, char **argv) {
#if 0
    //deamon process flow.
    pid_t pc;
    pc = fork();
    if(pc < 0 )
    {
        printf("error fork.\n");
        exit(1);
    }
    else if(pc > 0)
    {
        exit(1);//exit parent process.
    }
    /*else if(pc == 0)
    {
        setsid(); // escape control.
        chdir("/");
        umask(0);
        for(int32_t i = 0; i <= getdtablesize(); i++)
        {
            close(i);
        }
    }*/
#endif
  int ret;
  int seterrhdl = 1;
  struct timespec semtime;

  open_log("./v4l2_daemon.log");
  output_log(LOG_DEBUG, 0, "This is v4l2 daemon log file");
  char *level = NULL;
  char *daemon_logpath;
  level = getenv("HANTRO_LOG_LEVEL");
  if (NULL != level) {
    hantro_log_level = atoi(level);
  }
  if (0 == hantro_log_level) {
    hantro_log_level = 3;
  }
  daemon_logpath = getenv("DAEMON_LOGPATH");
  if (daemon_logpath != NULL)
    vsidaemonstdlog = freopen(daemon_logpath, "w", stdout);

  vsi_v4l2_cmd vsi_v4l2_cmd_p;
  int32_t v4l2_msg_size = sizeof(vsi_v4l2_msg);
  struct vsi_v4l2_dev_info hwinfo = {0};
  get_codec_info(&hwinfo);

  v4l2_daemon_inst *cur_inst_p = NULL;
  hash_table_init(&ht);
  maintid = pthread_self();
  HANTRO_LOG(HANTRO_LEVEL_INFO, "main thread id = %lx\n", maintid);
  ret = register_sighandler();
  if (ret < 0) return -1;

  pipe_fd = open_v4l2_device();
  mmap_fd = pipe_fd;
  // need to inform kernel driver hw config here
  ioctl(pipe_fd, VSI_IOCTL_CMD_INITDEV, &hwinfo);

  while (1) {
    ret = 0;
    if (seterrhdl) {
      ret = sigsetjmp(mainsigbuf, 1);
      seterrhdl = 0;
      if (ret) {
        HANTRO_LOG(HANTRO_LEVEL_ERROR, "sigsetjmp failed, error %d.\n", ret);
        break;
      }
    }

    check_zombieinst();
    /*  get message.  */
    ret = receive_from_v4l2(pipe_fd, &vsi_v4l2_cmd_p.msg, v4l2_msg_size);
    if (ret <= 0) {
      if (ret == -EBADF) goto exit;
      continue;
    }

    if (vsi_v4l2_cmd_p.msg.inst_id != 0) {
      /* get a valid command */
      /* find instance accordingly. */
      if (hash_table_find(&ht, vsi_v4l2_cmd_p.msg.inst_id, &cur_inst_p) != 0) {
        /* not find, maybe a new inst */
        if (hash_table_insert(&ht, vsi_v4l2_cmd_p.msg.inst_id) != 0) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR,
                     "instance not found, but insert unsuccessful.\n");
          ASSERT(0);  // insert unsuccessful.
        }

        /* create instance */
        if (hash_table_find(&ht, vsi_v4l2_cmd_p.msg.inst_id, &cur_inst_p) != 0) {
          HANTRO_LOG(HANTRO_LEVEL_ERROR, "instance not found after create.\n");
          ASSERT(0);
        }
        if (inst_init(cur_inst_p, &vsi_v4l2_cmd_p.msg) < 0) {
          hash_table_remove(&ht, vsi_v4l2_cmd_p.msg.inst_id, 1);
          vsi_v4l2_cmd_p.msg.size = 0;
          send_ack_to_v4l2(pipe_fd, &vsi_v4l2_cmd_p.msg,
                           sizeof(struct vsi_v4l2_msg_hdr));  // ack an ok;
          send_fatalerror_orphan_msg(vsi_v4l2_cmd_p.msg.inst_id, DAEMON_ERR_INST_CREATE);
          continue;
        }
      }

      check_instance(cur_inst_p);

      /*  send command to worker. */
      if (cur_inst_p->flag & (1 << INST_FATAL_ERROR)) {
        if (vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_DESTROY_ENC
          || vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_DESTROY_DEC
          || vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_EXIT) {
          pthread_kill(cur_inst_p->tid, SIGKILL);
          //inst_destroy is not reliable in this condition
          hash_table_remove(&ht, vsi_v4l2_cmd_p.msg.inst_id, 1);
        }
        vsi_v4l2_cmd_p.msg.size = 0;
        vsi_v4l2_cmd_p.msg.error = DAEMON_ERR_DEC_FATAL_ERROR;
        send_ack_to_v4l2(pipe_fd, &vsi_v4l2_cmd_p.msg, sizeof(struct vsi_v4l2_msg_hdr));
        if (vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_EXIT)
          goto exit;
      } else {
        send_cmd_to_worker(cur_inst_p, &vsi_v4l2_cmd_p.msg);

        /* if get STREAMOFF, wait worker's semphore, and destory inst.*/
        if ((vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_DESTROY_ENC)
            //                || (vsi_v4l2_cmd_p.msg.cmd_id ==
            //                V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE)
            || (vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_DESTROY_DEC) ||
            (vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_EXIT)) {
          if ((cur_inst_p->func.in_source_change == NULL) ||
              cur_inst_p->func.in_source_change(cur_inst_p->codec) == 0) {
            clock_gettime(CLOCK_REALTIME, &semtime);
            semtime.tv_nsec += 50000000;	//20ms should be enough for normal exit
            ret = sem_timedwait(&cur_inst_p->sem_done, &semtime);
            if (ret != 0) {
              HANTRO_LOG(HANTRO_LEVEL_WARNING, "wait %lx exit timeout\n", cur_inst_p->instance_id);

              hash_table_remove(&ht, vsi_v4l2_cmd_p.msg.inst_id, 0);
              if (add_zombieinst(cur_inst_p, &semtime)) {
                  inst_destroy(cur_inst_p);
                  free(cur_inst_p);
              }
            } else {
              inst_destroy(cur_inst_p);
              hash_table_remove(&ht, vsi_v4l2_cmd_p.msg.inst_id, 1);
            }
            if (vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_EXIT) {
              goto exit;
            }
          }
        }
      }
    } else if (vsi_v4l2_cmd_p.msg.cmd_id == V4L2_DAEMON_VIDIOC_EXIT) {
      vsi_v4l2_cmd_p.msg.size = 0;
      // send_ack_to_v4l2(pipe_fd, &vsi_v4l2_cmd_p.msg, sizeof(struct
      // vsi_v4l2_msg_hdr));//ack an ok;
      goto exit;
    }

    vsi_v4l2_cmd_p.msg.inst_id = 0;  // make sure this value is read from pipe.
  }
exit:
  close(pipe_fd);
  removeall_zombieinst();
  if (vsidaemonstdlog) fclose(vsidaemonstdlog);
  return 0;
}

void *worker_threads(void *arg) {
  int ret = 0;
  v4l2_daemon_inst *h = (v4l2_daemon_inst *)arg;
  vsi_v4l2_cmd *v4l2_cmd = NULL;
  unsigned long inst_id = NO_RESPONSE_SEQID;
  int32_t proc_result = 0;

  ret = sigsetjmp(h->sigbuf, 1);
  if (ret != 0) {
    // send error msg to driver
    HANTRO_LOG(HANTRO_LEVEL_ERROR, " %lx catch exception \n", inst_id);
    if (h->flag & (1 << INST_ZOMBIE))
        h->flag |= (1 << INST_DEAD);
    if (h->flag & (1 << INST_FORCEEXIT))
        goto label_worker_done;
    if (h->instance_id != NO_RESPONSE_SEQID)
      send_fatalerror_orphan_msg(h->instance_id, DAEMON_ERR_SIGNAL_CONFIG);
    goto label_worker_done;
  }

  /*loop machine*/
  do {
    /* fetch new msg */
    FifoPop(h->fifo_inst, (FifoObject *)&v4l2_cmd, FIFO_EXCEPTION_DISABLE);
    if (inst_id == NO_RESPONSE_SEQID) {
      inst_id = v4l2_cmd->msg.inst_id;
    } else if (inst_id != v4l2_cmd->msg.inst_id) {
      HANTRO_LOG(HANTRO_LEVEL_ERROR,
                 "cmd and thread seqid mismatch 0x%lx-0x%lx.\n",
                 v4l2_cmd->msg.inst_id, inst_id);
    }
    h->pop_cnt++;
    HANTRO_LOG(HANTRO_LEVEL_LOG,
               "worker_threads inst[%ld]: codec_fmt %d, cmd id %d\n",
               h->instance_id, v4l2_cmd->msg.codec_fmt, v4l2_cmd->msg.cmd_id);

    /* process msg */
    proc_result = 0;
    if (h->func.proc) {
      proc_result = h->func.proc(h->codec, &v4l2_cmd->msg);
    }

    object_heap_free(&h->cmds, (object_base_p)v4l2_cmd);

    if (proc_result) {
      HANTRO_LOG(HANTRO_LEVEL_DEBUG, "Instance [%ld] codec finished all jobs\n",
                 h->instance_id);
      break;
    }

  } while (1);

label_worker_done:
  sem_post(&h->sem_done);
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "worker_threads finish, instance id %lx\n",
             h->instance_id);
  return NULL;
}

static void inst_set_format(v4l2_daemon_inst *h) {
  switch (h->codec_fmt) {
#ifdef HAS_VSI_ENC
    case V4L2_DAEMON_CODEC_ENC_HEVC:
    case V4L2_DAEMON_CODEC_ENC_H264:
    case V4L2_DAEMON_CODEC_ENC_AV1:
    case V4L2_DAEMON_CODEC_ENC_VP8:
    case V4L2_DAEMON_CODEC_ENC_VP9:
    case V4L2_DAEMON_CODEC_ENC_MPEG2:
    case V4L2_DAEMON_CODEC_ENC_JPEG:
      h->codec_mode = DAEMON_MODE_ENCODER;
      h->func.create = vsi_create_encoder;
      break;
#endif

    case V4L2_DAEMON_CODEC_DEC_HEVC:
    case V4L2_DAEMON_CODEC_DEC_H264:
    case V4L2_DAEMON_CODEC_DEC_JPEG:
    case V4L2_DAEMON_CODEC_DEC_VP9:
    case V4L2_DAEMON_CODEC_DEC_MPEG2:
    case V4L2_DAEMON_CODEC_DEC_MPEG4:
    case V4L2_DAEMON_CODEC_DEC_VP8:
    case V4L2_DAEMON_CODEC_DEC_H263:
    case V4L2_DAEMON_CODEC_DEC_VC1_G:
    case V4L2_DAEMON_CODEC_DEC_VC1_L:
    case V4L2_DAEMON_CODEC_DEC_RV:
    case V4L2_DAEMON_CODEC_DEC_AVS2:
    case V4L2_DAEMON_CODEC_DEC_XVID:
      h->codec_mode = DAEMON_MODE_DECODER;
      h->func.create = vsi_create_decoder;
      break;

    case V4L2_DAEMON_CODEC_UNKNOW_TYPE:
    default:
      h->codec_mode = DAEMON_MODE_UNKNOWN;
      h->func.create = NULL;
      ASSERT(0);
      break;
  }
}

static int32_t inst_init(v4l2_daemon_inst *h, vsi_v4l2_msg *v4l2_msg) {
  int ret = 0;
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "inst_init begin\n");
  ASSERT(h != NULL);
  memset(h, 0, sizeof(v4l2_daemon_inst));

  h->tid = 0;
  h->pop_cnt = 0;
  h->instance_id = v4l2_msg->inst_id;
  h->sequence_id = v4l2_msg->seq_id;
  h->codec_fmt = v4l2_msg->codec_fmt;

  inst_set_format(h);
  if (h->func.create == NULL) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "Unknown codec format.\n");
    return -1;
  }

  if (object_heap_init(&(h->cmds), sizeof(vsi_v4l2_cmd),
                       ENC_HEVC_H264_ID_OFFSET)) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "object_heap_init error.\n");
    return -1;
  }

  sem_init(&h->sem_done, 0, 0);
  FifoInit(MAX_FIFO_CAPACITY, &h->fifo_inst);

  h->codec = h->func.create(h);

  ret = pthread_attr_init(&h->attr);
  if (ret != 0) {
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "pthread_attr_init fail, ret=%d\n", ret);
    ret = -1;
    goto errout;
  }

  ret = pthread_create(&h->tid, &h->attr, &worker_threads, h);
  if (ret != 0) {
    pthread_attr_destroy(&h->attr);
    HANTRO_LOG(HANTRO_LEVEL_ERROR, "pthread_create fail, ret=%d\n", ret);
    ret = -1;
    goto errout;
  }
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "inst_init %lx end\n", h->tid);
  return 0;

errout:
  if (h->codec) h->func.destroy(h->codec);

  if (h->fifo_inst != NULL) FifoRelease(h->fifo_inst);
  destroy_heap(&(h->cmds), object_heap_free);
  sem_destroy(&h->sem_done);
  return ret;
}

static void destroy_heap(struct object_heap *heap,
                         void (*func)(struct object_heap *heap,
                                      struct object_base *object)) {
  struct object_base *object;
  object_heap_iterator iter;

  object = object_heap_first(heap, &iter);

  while (object) {
    if (func) func(heap, object);

    object = object_heap_next(heap, &iter);
  }

  object_heap_destroy(heap);
}

int inst_destroy(v4l2_daemon_inst *h) {
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "inst_destroy begin\n");

  if (h->tid != 0) {
    pthread_join(h->tid, NULL);
    pthread_attr_destroy(&h->attr);
  } else {
    HANTRO_LOG(HANTRO_LEVEL_DEBUG, "h->tid is %d \n", (uint32_t)h->tid);
  }

  /* codec destroy */
  if (h->codec) {
    if (!(h->flag & (1 << INST_CATCH_EXCEPTION)))
      h->func.destroy(h->codec);
    else
      HANTRO_LOG(HANTRO_LEVEL_WARNING, "inst %lx exit itself before destroy. \n", h->instance_id);
    free(h->codec);
    h->codec = NULL;
  }

  sem_destroy(&h->sem_done);

  if (h->fifo_inst != NULL) {
    FifoRelease(h->fifo_inst);
  } else {
    HANTRO_LOG(HANTRO_LEVEL_DEBUG, "h->fifo_inst is NULL. \n");
  }

  // object_heap_destroy(&(h->cmds));
  destroy_heap(&(h->cmds), object_heap_free);
  HANTRO_LOG(HANTRO_LEVEL_DEBUG, "inst_destroy finish\n");
  return 0;
}
