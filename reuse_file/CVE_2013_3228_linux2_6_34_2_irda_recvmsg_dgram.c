static int CVE_2013_3228_linux2_6_34_2_irda_recvmsg_dgram(struct kiocb *iocb, struct socket *sock,
			      struct msghdr *msg, size_t size, int flags)
{
	struct sock *sk = sock->sk;
	struct irda_sock *self = irda_sk(sk);
	struct sk_buff *skb;
	size_t copied;
	int err;

	IRDA_DEBUG(4, "%s()\n", __func__);

	lock_kernel();
	if ((err = sock_error(sk)) < 0)
		goto out;

	skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT,
				flags & MSG_DONTWAIT, &err);
	if (!skb)
		goto out;

	skb_reset_transport_header(skb);
	copied = skb->len;

	if (copied > size) {
		IRDA_DEBUG(2, "%s(), Received truncated frame (%zd < %zd)!\n",
			   __func__, copied, size);
		copied = size;
		msg->msg_flags |= MSG_TRUNC;
	}
	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);

	skb_free_datagram(sk, skb);

	/*
	 *  Check if we have previously stopped IrTTP and we know
	 *  have more free space in our rx_queue. If so tell IrTTP
	 *  to start delivering frames again before our rx_queue gets
	 *  empty
	 */
	if (self->rx_flow == FLOW_STOP) {
		if ((atomic_read(&sk->sk_rmem_alloc) << 2) <= sk->sk_rcvbuf) {
			IRDA_DEBUG(2, "%s(), Starting IrTTP\n", __func__);
			self->rx_flow = FLOW_START;
			irttp_flow_request(self->tsap, FLOW_START);
		}
	}
	unlock_kernel();
	return copied;

out:
	unlock_kernel();
	return err;
}