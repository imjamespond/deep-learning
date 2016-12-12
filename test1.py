# -*- coding: utf-8 -*- 
'''
Created on Nov 29, 2016

@author: james
'''

import time
import tensorflow as tf
import numpy as np
from PIL import Image
#import com.tf.utils as utils
from com.tf import utils

# Basic model parameters as external flags.
flags = tf.app.flags
FLAGS = flags.FLAGS
flags.DEFINE_float('learning_rate', 0.01, 'Initial learning rate.')
flags.DEFINE_integer('max_steps', 200, 'Number of steps to run trainer.')
flags.DEFINE_integer('hidden1', 128, 'Number of units in hidden layer 1.')
flags.DEFINE_integer('hidden2', 32, 'Number of units in hidden layer 2.')
flags.DEFINE_integer('batch_size', 4, 'Batch size. Must divide evenly into the dataset sizes.')
flags.DEFINE_string('train_dir', 'data', 'Directory to put the training data.')
flags.DEFINE_boolean('fake_data', False, 'If true, uses fake data for unit testing.')
NUM_CLASSES = 2 
IMAGE_SIZE = 28 
CHANNELS = 1#jpeg got rpg namely 3 ,while png is 1
IMAGE_PIXELS = IMAGE_SIZE * IMAGE_SIZE * CHANNELS
IMAGE_FILES = ['img/2-1.png', 'img/2-2.png', 'img/2-7.png', 'img/2-4.png']

#
train_images = []
for filename in IMAGE_FILES:
  image = Image.open(filename)#读取image数据
  image = image.resize((IMAGE_SIZE,IMAGE_SIZE))#resize图像
  image = np.array(image)#通过numpy得到数组
  train_images.append(image)
  #print(image)

train_images = np.array(train_images)#展开rpg
print(train_images)
train_images = train_images.reshape(4, IMAGE_PIXELS)#重塑

label = [0,1,1,1]
train_labels = np.array(label)

def fill_feed_dict(images_feed,labels_feed, images_pl, labels_pl):
  feed_dict = {
      images_pl: images_feed,
      labels_pl: labels_feed,
  }
  return feed_dict

def do_eval(sess,
            eval_correct,
            images_placeholder,
            labels_placeholder,
            data_set):
  # And run one epoch of eval.
  true_count = 0  # Counts the number of correct predictions.
  steps_per_epoch = 4 // FLAGS.batch_size
  num_examples = steps_per_epoch * FLAGS.batch_size
  for step in xrange(steps_per_epoch):
    feed_dict = fill_feed_dict(train_images, train_labels, images_placeholder, labels_placeholder)
    true_count += sess.run(eval_correct, feed_dict=feed_dict)
  precision = true_count / num_examples
  print('  Num examples: %d  Num correct: %d  Precision @ 1: %0.04f' %
        (num_examples, true_count, precision))

def run_training():
  with tf.Graph().as_default():
    #持有image,label数据
    images_placeholder, labels_placeholder = utils.placeholder_inputs(4, IMAGE_PIXELS)
    #推测模型
    logits = utils.inference(images_placeholder, FLAGS.hidden1, FLAGS.hidden2, IMAGE_PIXELS, NUM_CLASSES)
    #输出丢失计算
    loss = utils.cal_loss(logits, labels_placeholder)
    #输出应用梯度
    train_op = utils.training(loss, FLAGS.learning_rate)
    #输出对比罗杰斯和label(在评估中)
    eval_correct = utils.evaluation(logits, labels_placeholder)
    #保存训练的checkpoint
    saver = tf.train.Saver()
    
    sess = tf.Session()
    init = tf.initialize_all_variables()
    sess.run(init)
    
    for step in xrange(FLAGS.max_steps):
      start_time = time.time()
      feed_dict = fill_feed_dict(train_images, train_labels, images_placeholder, labels_placeholder)
      _, loss_value = sess.run([train_op, loss], feed_dict=feed_dict)
      duration = time.time() - start_time
      
      if step % 10 == 0:
        # Print status to stdout.
        print('Step %d: loss = %.2f (%.3f sec)' % (step, loss_value, duration))
      if (step + 1) % 1000 == 0 or (step + 1) == FLAGS.max_steps:
        saver.save(sess, FLAGS.train_dir, global_step=step)
        print('Training Data Eval:')
        do_eval(sess, eval_correct, images_placeholder, labels_placeholder, train_images)

def main(_):
  run_training()
if __name__ == '__main__':
  tf.app.run()