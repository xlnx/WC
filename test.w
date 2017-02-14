int test()
{
	int arr[10];
	*arr = 1, *(arr + 1) = 1;
	int i = 0;
	while (i < 10)
		arr[i] += arr[i - 1] + arr[i - 2];
	return arr[9];
}

int main()
{
	int a = test();
	return 0;
}
